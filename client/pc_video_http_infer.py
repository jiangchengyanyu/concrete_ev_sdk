import argparse
import time
from pathlib import Path

import cv2
import requests
from tqdm import tqdm


def draw_detections(frame, event):
    objects = []

    if isinstance(event, dict):
        algorithm_data = event.get("algorithm_data", {})
        objects = algorithm_data.get("target_info", [])

        if not objects:
            model_data = event.get("model_data", {})
            objects = model_data.get("objects", [])

    for obj in objects:
        x = int(obj.get("x", 0))
        y = int(obj.get("y", 0))
        w = int(obj.get("width", 0))
        h = int(obj.get("height", 0))
        name = obj.get("name", "obj")
        conf = float(obj.get("confidence", 0.0))

        color = (0, 255, 0) if name == "excellent" else (0, 0, 255)

        x1 = max(0, x)
        y1 = max(0, y)
        x2 = max(0, x + w)
        y2 = max(0, y + h)

        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 4)

        label = f"{name} {conf:.2f}"
        text_y = max(30, y1 - 8)
        cv2.putText(
            frame,
            label,
            (x1, text_y),
            cv2.FONT_HERSHEY_SIMPLEX,
            1.0,
            color,
            3,
            cv2.LINE_AA,
        )

    return frame


def infer_frame(server, frame, jpeg_quality=85, timeout=30):
    ok, encoded = cv2.imencode(
        ".jpg",
        frame,
        [int(cv2.IMWRITE_JPEG_QUALITY), int(jpeg_quality)]
    )
    if not ok:
        raise RuntimeError("failed to encode frame")

    files = {
        "file": ("frame.jpg", encoded.tobytes(), "image/jpeg")
    }

    resp = requests.post(
        server.rstrip("/") + "/infer",
        files=files,
        timeout=timeout
    )
    resp.raise_for_status()
    data = resp.json()

    return data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", default="http://192.168.1.2:8899")
    parser.add_argument("--source", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--seconds", type=float, default=30.0)
    parser.add_argument("--stride", type=int, default=2)
    parser.add_argument("--jpeg-quality", type=int, default=85)
    args = parser.parse_args()

    source = Path(args.source)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    print("server:", args.server)
    print("source:", source)
    print("out:", out_path)
    print("seconds:", args.seconds)
    print("stride:", args.stride)

    health = requests.get(args.server.rstrip("/") + "/health", timeout=5)
    print("health:", health.json())

    cap = cv2.VideoCapture(str(source))
    if not cap.isOpened():
        raise RuntimeError(f"failed to open video: {source}")

    fps = cap.get(cv2.CAP_PROP_FPS)
    if fps <= 0:
        fps = 25.0

    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

    max_frames = int(args.seconds * fps)

    print(f"video info: {width}x{height}, fps={fps:.2f}, total_frames={total_frames}")
    print(f"will process frames: {max_frames}")

    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(str(out_path), fourcc, fps, (width, height))
    if not writer.isOpened():
        raise RuntimeError(f"failed to create writer: {out_path}")

    last_event = None
    frame_idx = 0
    written = 0
    infer_count = 0
    fail_count = 0

    t0 = time.time()

    pbar = tqdm(total=max_frames)

    while written < max_frames:
        ok, frame = cap.read()
        if not ok:
            break

        do_infer = (frame_idx % args.stride == 0) or last_event is None

        if do_infer:
            try:
                data = infer_frame(
                    args.server,
                    frame,
                    jpeg_quality=args.jpeg_quality,
                )
                last_event = data.get("event", {})
                infer_count += 1

                targets = (
                    last_event.get("algorithm_data", {})
                    .get("target_info", [])
                    if isinstance(last_event, dict)
                    else []
                )

                if targets:
                    first = targets[0]
                    tqdm.write(
                        f"frame={frame_idx}, "
                        f"name={first.get('name')}, "
                        f"conf={float(first.get('confidence', 0)):.3f}"
                    )
            except Exception as e:
                fail_count += 1
                tqdm.write(f"[WARN] frame={frame_idx}, infer failed: {e}")

        out_frame = frame.copy()
        if last_event is not None:
            out_frame = draw_detections(out_frame, last_event)

        writer.write(out_frame)

        frame_idx += 1
        written += 1
        pbar.update(1)

    pbar.close()

    cap.release()
    writer.release()

    elapsed = time.time() - t0

    print("done")
    print("saved:", out_path)
    print("written frames:", written)
    print("infer count:", infer_count)
    print("fail count:", fail_count)
    print(f"elapsed: {elapsed:.2f}s")
    print(f"write fps: {written / elapsed:.2f}")
    print(f"infer fps: {infer_count / elapsed:.2f}")


if __name__ == "__main__":
    main()