import ctypes
import cv2
import numpy as np
import os
import time
import argparse

LIB_PATH = "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/build/libji.so"
MODEL_PATH = "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om"

JI_IMAGE_TYPE_YUV420 = 1
JI_UNSIGNED_CHAR = 2

def align_up(num, align):
    return (num + align - 1) & ~(align - 1)

class JiImageInfo(ctypes.Structure):
    _fields_ = [
        ("nWidth", ctypes.c_uint),
        ("nHeight", ctypes.c_uint),
        ("nWidthStride", ctypes.c_uint),
        ("nHeightStride", ctypes.c_uint),
        ("nFrameRate", ctypes.c_uint),
        ("dwTimeStamp", ctypes.c_ulong),
        ("pData", ctypes.c_void_p),
        ("nDataLen", ctypes.c_uint),
        ("nFormat", ctypes.c_int),
        ("nDataType", ctypes.c_int),
        ("nFrameNo", ctypes.c_uint),
        ("byRes", ctypes.c_ubyte * 4),
    ]

class JiEvent(ctypes.Structure):
    _fields_ = [
        ("code", ctypes.c_int),
        ("json", ctypes.c_char_p),
    ]

def bgr_to_nv12_aligned(bgr):
    h, w = bgr.shape[:2]
    aw = align_up(w, 16)
    ah = align_up(h, 2)

    padded = np.zeros((ah, aw, 3), dtype=np.uint8)
    padded[:h, :w, :] = bgr

    yuv_i420 = cv2.cvtColor(padded, cv2.COLOR_BGR2YUV_I420)

    y_size = ah * aw
    uv_size = y_size // 4

    flat = yuv_i420.reshape(-1)
    y = flat[:y_size].reshape(ah, aw)
    u = flat[y_size:y_size + uv_size].reshape(ah // 2, aw // 2)
    v = flat[y_size + uv_size:y_size + uv_size * 2].reshape(ah // 2, aw // 2)

    nv12 = np.zeros((ah * 3 // 2, aw), dtype=np.uint8)
    nv12[:ah, :] = y

    nv12_uv = nv12[ah:, :].reshape(ah // 2, aw // 2, 2)
    nv12_uv[:, :, 0] = u
    nv12_uv[:, :, 1] = v

    return np.ascontiguousarray(nv12), aw, ah

def load_lib():
    os.environ["CONCRETE_MODEL_PATH"] = MODEL_PATH

    lib = ctypes.CDLL(LIB_PATH)

    lib.ji_init.argtypes = [ctypes.c_int, ctypes.c_void_p]
    lib.ji_init.restype = ctypes.c_int

    lib.ji_create_predictor.argtypes = [ctypes.c_int]
    lib.ji_create_predictor.restype = ctypes.c_void_p

    lib.ji_destroy_predictor.argtypes = [ctypes.c_void_p]
    lib.ji_destroy_predictor.restype = None

    lib.ji_reinit.argtypes = []
    lib.ji_reinit.restype = None

    lib.ji_calc_image.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(JiImageInfo),
        ctypes.c_uint,
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.POINTER(JiImageInfo)),
        ctypes.POINTER(ctypes.c_uint),
        ctypes.POINTER(JiEvent),
    ]
    lib.ji_calc_image.restype = ctypes.c_int

    return lib

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--out", default="data/concrete/ji_video_result.mp4")
    parser.add_argument("--max-frames", type=int, default=100)
    parser.add_argument("--stride", type=int, default=1)
    args = parser.parse_args()

    lib = load_lib()

    print("ji_init:", lib.ji_init(0, None))
    predictor = lib.ji_create_predictor(0)
    print("predictor:", predictor)

    if not predictor:
        raise RuntimeError("ji_create_predictor failed")

    cap = cv2.VideoCapture(args.source)
    if not cap.isOpened():
        raise RuntimeError(f"failed to open video: {args.source}")

    src_fps = cap.get(cv2.CAP_PROP_FPS)
    if src_fps <= 0:
        src_fps = 25.0

    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    total = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

    print(f"video info: {width}x{height}, fps={src_fps:.2f}, frames={total}")

    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(args.out, fourcc, src_fps, (width, height))
    if not writer.isOpened():
        raise RuntimeError(f"failed to open writer: {args.out}")

    frame_idx = 0
    written = 0
    infer_frames = 0
    last_out = None

    t0 = time.time()

    while True:
        ok, frame = cap.read()
        if not ok:
            break

        if args.max_frames > 0 and written >= args.max_frames:
            break

        do_infer = (frame_idx % args.stride == 0) or last_out is None

        if do_infer:
            h, w = frame.shape[:2]
            nv12, aw, ah = bgr_to_nv12_aligned(frame)

            in_frame = JiImageInfo()
            in_frame.nWidth = w
            in_frame.nHeight = h
            in_frame.nWidthStride = aw
            in_frame.nHeightStride = ah
            in_frame.nFrameRate = int(src_fps)
            in_frame.dwTimeStamp = frame_idx
            in_frame.pData = nv12.ctypes.data_as(ctypes.c_void_p)
            in_frame.nDataLen = nv12.nbytes
            in_frame.nFormat = JI_IMAGE_TYPE_YUV420
            in_frame.nDataType = JI_UNSIGNED_CHAR
            in_frame.nFrameNo = frame_idx

            out_frames = ctypes.POINTER(JiImageInfo)()
            out_count = ctypes.c_uint(0)
            event = JiEvent()

            ret = lib.ji_calc_image(
                ctypes.c_void_p(predictor),
                ctypes.byref(in_frame),
                1,
                None,
                ctypes.byref(out_frames),
                ctypes.byref(out_count),
                ctypes.byref(event),
            )

            if ret != 0 or out_count.value <= 0:
                print(f"[WARN] frame={frame_idx}, ji_calc_image failed, ret={ret}")
                out_bgr = frame
            else:
                out0 = out_frames[0]
                out_h = out0.nHeight
                out_w = out0.nWidth
                stride = out0.nWidthStride

                raw_type = ctypes.c_ubyte * out0.nDataLen
                raw = raw_type.from_address(out0.pData)
                arr = np.frombuffer(raw, dtype=np.uint8)

                img_stride = arr.reshape(out_h, stride)
                out_bgr = img_stride[:, :out_w * 3].reshape(out_h, out_w, 3).copy()

                if event.json:
                    print(f"frame={frame_idx}, code={event.code}, json={event.json.decode('utf-8')}")

            last_out = out_bgr
            infer_frames += 1
        else:
            out_bgr = last_out

        writer.write(out_bgr)
        written += 1
        frame_idx += 1

        if written % 20 == 0:
            elapsed = time.time() - t0
            print(f"written={written}, infer_frames={infer_frames}, avg_fps={written / elapsed:.2f}")

    elapsed = time.time() - t0

    cap.release()
    writer.release()

    lib.ji_destroy_predictor(ctypes.c_void_p(predictor))
    lib.ji_reinit()

    print("done")
    print(f"saved: {args.out}")
    print(f"written frames: {written}")
    print(f"infer frames: {infer_frames}")
    print(f"elapsed: {elapsed:.2f}s")
    print(f"average fps: {written / elapsed:.2f}")

if __name__ == "__main__":
    main()
