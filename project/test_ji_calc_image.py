import ctypes
import cv2
import numpy as np
import os

LIB_PATH = "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/build/libji.so"
IMG_PATH = "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/data/concrete/test.jpg"
OUT_PATH = "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/data/concrete/ji_result.jpg"

os.environ["CONCRETE_MODEL_PATH"] = "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om"

JI_IMAGE_TYPE_BGR = 0
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

bgr = cv2.imread(IMG_PATH)
if bgr is None:
    raise RuntimeError(f"failed to read image: {IMG_PATH}")

h, w = bgr.shape[:2]
nv12, aw, ah = bgr_to_nv12_aligned(bgr)

in_frame = JiImageInfo()
in_frame.nWidth = w
in_frame.nHeight = h
in_frame.nWidthStride = aw
in_frame.nHeightStride = ah
in_frame.nFrameRate = 25
in_frame.dwTimeStamp = 0
in_frame.pData = nv12.ctypes.data_as(ctypes.c_void_p)
in_frame.nDataLen = nv12.nbytes
in_frame.nFormat = JI_IMAGE_TYPE_YUV420
in_frame.nDataType = JI_UNSIGNED_CHAR
in_frame.nFrameNo = 0

out_frames = ctypes.POINTER(JiImageInfo)()
out_count = ctypes.c_uint(0)
event = JiEvent()

print("ji_init:", lib.ji_init(0, None))
predictor = lib.ji_create_predictor(0)
print("predictor:", predictor)

if not predictor:
    raise RuntimeError("ji_create_predictor failed")

ret = lib.ji_calc_image(
    ctypes.c_void_p(predictor),
    ctypes.byref(in_frame),
    1,
    None,
    ctypes.byref(out_frames),
    ctypes.byref(out_count),
    ctypes.byref(event),
)

print("ji_calc_image ret:", ret)
print("out_count:", out_count.value)
print("event.code:", event.code)
print("event.json:", event.json.decode("utf-8") if event.json else None)

if ret == 0 and out_count.value > 0:
    out0 = out_frames[0]
    out_h = out0.nHeight
    out_w = out0.nWidth
    stride = out0.nWidthStride

    raw_type = ctypes.c_ubyte * out0.nDataLen
    raw = raw_type.from_address(out0.pData)
    arr = np.frombuffer(raw, dtype=np.uint8)

    img_stride = arr.reshape(out_h, stride)
    out_bgr = img_stride[:, :out_w * 3].reshape(out_h, out_w, 3).copy()

    cv2.imwrite(OUT_PATH, out_bgr)
    print("saved:", OUT_PATH)

lib.ji_destroy_predictor(ctypes.c_void_p(predictor))
lib.ji_reinit()
print("done")
