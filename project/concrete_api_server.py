import ctypes
import cv2
import numpy as np
import os
import time
import json
import threading
from pathlib import Path
from fastapi import FastAPI, UploadFile, File
from fastapi.responses import FileResponse, JSONResponse
import uvicorn

PROJECT_DIR = Path("/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy")
LIB_PATH = str(PROJECT_DIR / "build/libji.so")
MODEL_PATH = str(PROJECT_DIR / "model/concrete/best.om")
RESULT_DIR = PROJECT_DIR / "api_results"
RESULT_DIR.mkdir(exist_ok=True)

os.environ["CONCRETE_MODEL_PATH"] = MODEL_PATH

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

class ConcreteSDK:
    def __init__(self):
        self.lib = ctypes.CDLL(LIB_PATH)

        self.lib.ji_init.argtypes = [ctypes.c_int, ctypes.c_void_p]
        self.lib.ji_init.restype = ctypes.c_int

        self.lib.ji_create_predictor.argtypes = [ctypes.c_int]
        self.lib.ji_create_predictor.restype = ctypes.c_void_p

        self.lib.ji_destroy_predictor.argtypes = [ctypes.c_void_p]
        self.lib.ji_destroy_predictor.restype = None

        self.lib.ji_reinit.argtypes = []
        self.lib.ji_reinit.restype = None

        self.lib.ji_calc_image.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(JiImageInfo),
            ctypes.c_uint,
            ctypes.c_char_p,
            ctypes.POINTER(ctypes.POINTER(JiImageInfo)),
            ctypes.POINTER(ctypes.c_uint),
            ctypes.POINTER(JiEvent),
        ]
        self.lib.ji_calc_image.restype = ctypes.c_int

        ret = self.lib.ji_init(0, None)
        if ret != 0:
            raise RuntimeError(f"ji_init failed: {ret}")

        self.predictor = self.lib.ji_create_predictor(0)
        if not self.predictor:
            raise RuntimeError("ji_create_predictor failed")

        self.lock = threading.Lock()

    def infer(self, bgr):
        h, w = bgr.shape[:2]
        nv12, aw, ah = bgr_to_nv12_aligned(bgr)

        in_frame = JiImageInfo()
        in_frame.nWidth = w
        in_frame.nHeight = h
        in_frame.nWidthStride = aw
        in_frame.nHeightStride = ah
        in_frame.nFrameRate = 25
        in_frame.dwTimeStamp = int(time.time() * 1000)
        in_frame.pData = nv12.ctypes.data_as(ctypes.c_void_p)
        in_frame.nDataLen = nv12.nbytes
        in_frame.nFormat = JI_IMAGE_TYPE_YUV420
        in_frame.nDataType = JI_UNSIGNED_CHAR
        in_frame.nFrameNo = 0

        out_frames = ctypes.POINTER(JiImageInfo)()
        out_count = ctypes.c_uint(0)
        event = JiEvent()

        with self.lock:
            ret = self.lib.ji_calc_image(
                ctypes.c_void_p(self.predictor),
                ctypes.byref(in_frame),
                1,
                None,
                ctypes.byref(out_frames),
                ctypes.byref(out_count),
                ctypes.byref(event),
            )

        if ret != 0 or out_count.value <= 0:
            return ret, None, {"code": -1, "json": None}

        out0 = out_frames[0]
        raw_type = ctypes.c_ubyte * out0.nDataLen
        raw = raw_type.from_address(out0.pData)
        arr = np.frombuffer(raw, dtype=np.uint8)

        img_stride = arr.reshape(out0.nHeight, out0.nWidthStride)
        out_bgr = img_stride[:, :out0.nWidth * 3].reshape(out0.nHeight, out0.nWidth, 3).copy()

        event_json = event.json.decode("utf-8") if event.json else "{}"

        try:
            parsed = json.loads(event_json)
        except Exception:
            parsed = {"raw": event_json}

        return ret, out_bgr, {
            "code": int(event.code),
            "json": parsed
        }

sdk = ConcreteSDK()
app = FastAPI(title="Concrete Quality Detection API")

@app.get("/health")
def health():
    return {
        "status": "ok",
        "model": MODEL_PATH,
        "lib": LIB_PATH
    }

@app.post("/infer")
async def infer(file: UploadFile = File(...)):
    data = await file.read()
    arr = np.frombuffer(data, dtype=np.uint8)
    bgr = cv2.imdecode(arr, cv2.IMREAD_COLOR)

    if bgr is None:
        return JSONResponse(
            status_code=400,
            content={"error": "failed to decode image"}
        )

    t0 = time.time()
    ret, out_bgr, event = sdk.infer(bgr)
    cost_ms = round((time.time() - t0) * 1000, 2)

    if ret != 0 or out_bgr is None:
        return JSONResponse(
            status_code=500,
            content={"error": "inference failed", "ret": ret}
        )

    name = f"result_{int(time.time() * 1000)}.jpg"
    out_path = RESULT_DIR / name
    cv2.imwrite(str(out_path), out_bgr)

    return {
        "ret": ret,
        "cost_ms": cost_ms,
        "event_code": event["code"],
        "event": event["json"],
        "result_url": f"/result/{name}"
    }

@app.get("/result/{name}")
def result(name: str):
    path = RESULT_DIR / name
    if not path.exists():
        return JSONResponse(status_code=404, content={"error": "not found"})
    return FileResponse(str(path), media_type="image/jpeg")

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8899)
