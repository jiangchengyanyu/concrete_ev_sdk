# QA200A2-B 混凝土质量检测 EV_SDK 接口与电脑端调用说明

## 1. 文档目的

本文档用于说明当前 QA200A2-B 后羿智盒上的混凝土质量检测部署方式、`libji.so` 标准接口定义、HTTP API 调用方式，以及电脑端调用盒子完成图片/视频推理的方法。

当前版本已经完成以下验证：

- `best.om` 可在 QA200A2-B / Ascend 310B1 上正常加载。
- Python 单图推理、视频推理已跑通。
- 已绕过外部 EVDeploy runtime，直接基于 AscendCL 封装 EV_SDK 标准 `libji.so`。
- `ji_create_predictor` 可加载 `best.om`。
- `ji_calc_image` 可完成真实图片推理，并返回 JSON 事件和带框结果图。
- 电脑端可通过 HTTP API 调用盒子完成图片推理。
- 电脑端可读取本地视频，按帧调用盒子 HTTP API，合成推理后的视频。

---

## 2. 当前部署架构

当前系统采用“电脑端请求 + 盒子端推理”的架构：

```text
Windows 电脑
  ├── 上传图片 / 视频帧
  ├── 调用 HTTP API
  └── 接收 JSON 与结果图
        │
        ▼
QA200A2-B 后羿智盒
  ├── FastAPI HTTP 服务，端口 8899
  ├── libji.so 标准接口库
  ├── AscendCL 直接加载 best.om
  ├── YOLO11 混凝土质量检测后处理
  └── Ascend 310B1 NPU 推理
```

需要注意：

- 电脑不能直接加载 `libji.so`。
- `libji.so` 是在盒子上编译的 ARM64 + AscendCL 动态库。
- 实际推理必须在 QA200A2-B 的 NPU 上完成。
- 电脑端应通过 HTTP 请求调用盒子上的服务。

---

## 3. 核心目录结构

当前工程目录：

```bash
/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy
```

核心文件如下：

```text
ev_sdk_demo4.0_evdeploy/
├── build/
│   └── libji.so                         # 已生成的 EV_SDK 标准接口库
├── model/concrete/
│   ├── best.om                          # Ascend OM 模型
│   ├── best.onnx                        # ONNX 模型备份
│   └── labels.txt                       # 类别文件
├── data/concrete/
│   ├── test.jpg                         # 测试图片
│   ├── ji_result.jpg                    # ji_calc_image 单图测试结果
│   └── *.mp4                            # 测试视频与输出视频
├── src/
│   ├── ji.cpp                           # EV_SDK 标准入口实现
│   ├── sample_algorithm.cpp/.h          # 算法业务逻辑、JSON 输出、画框
│   └── sample_detector.cpp/.h           # AscendCL 加载 best.om、前后处理
├── concrete_api_server.py               # 盒子端 HTTP 服务
├── test_ji_calc_image.py                # 单图接口测试脚本
├── test_ji_video.py                     # 盒子本地视频接口测试脚本
└── scripts_build_no_evdeploy.sh         # no-EVDeploy 版本编译脚本
```

当前电脑端视频 HTTP 调用脚本位置：

```text
D:\视频\pc_video_http_infer.py
```

---

## 4. 模型信息

当前模型来自 YOLO11 混凝土质量检测训练结果：

```text
best.pt → best.onnx → best.om
```

类别定义：

```text
0 → excellent
1 → good
```

当前 OM 模型输入输出已经验证：

```text
输入：1 × 3 × 640 × 640，FP16
输入字节数：2457600 bytes

输出：1 × 6 × 8400，FP16
输出字节数：100800 bytes
```

输出 6 个通道含义：

```text
0: x_center
1: y_center
2: width
3: height
4: excellent score
5: good score
```

注意：类别分数已经是概率值，不需要再做 sigmoid。

---

## 5. libji.so 接口说明

当前 `build/libji.so` 对外导出 EV_SDK 标准 C 接口。已验证导出函数包括：

```text
ji_get_version
ji_init
ji_reinit
ji_create_predictor
ji_destroy_predictor
ji_calc_image
ji_update_config
ji_calc_image_asyn
ji_set_callback
ji_create_face_db
ji_delete_face_db
ji_get_face_db_info
ji_face_add
ji_face_delete
ji_face_update
```

实际使用的核心接口如下。

### 5.1 获取版本

```cpp
JiErrorCode ji_get_version(char *pVersion);
```

作用：返回 SDK 与算法版本信息。

当前返回示例：

```json
{
  "sdk_version": "4.0.0",
  "algo_version": "1.0.1"
}
```

---

### 5.2 初始化 SDK

```cpp
JiErrorCode ji_init(int argc, char **argv);
```

当前 no-EVDeploy 版本中，`ji_init` 不再初始化 EVDeploy。模型加载放在 `ji_create_predictor` 内部完成。

调用成功返回：

```text
JISDK_RET_SUCCEED = 0
```

---

### 5.3 创建算法实例

```cpp
void* ji_create_predictor(JiPredictorType pdtype);
```

作用：创建算法实例，并在内部完成：

```text
SampleAlgorithm::Init
→ SampleDetector::Init
→ aclInit
→ aclrtSetDevice
→ aclmdlLoadFromFile(best.om)
→ 创建输入 / 输出 Dataset
```

当前模型路径优先由环境变量指定：

```bash
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om
```

如果不指定，会自动尝试以下路径：

```text
/usr/local/ev_sdk/model/concrete/best.om
/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om
model/concrete/best.om
```

成功日志示例：

```text
Concrete detector model path: .../model/concrete/best.om
model input size=2457600, output size=100800
Concrete detector init OK
Concrete SampleAlgorithm init OK
SamplePredictor init OK
```

---

### 5.4 同步单帧推理接口

```cpp
JiErrorCode ji_calc_image(
    void* predictor,
    const JiImageInfo* pInFrames,
    const unsigned int nInCount,
    const char* args,
    JiImageInfo **pOutFrames,
    unsigned int &nOutCount,
    JiEvent &event
);
```

这是当前最重要的推理入口。

功能：

```text
输入单帧图像
→ 转 BGR
→ YOLO11 前处理
→ AscendCL 推理
→ 后处理 + NMS
→ 输出带框图
→ 输出 JSON 结构化结果
```

当前主要使用方式：

```text
nInCount = 1
pInFrames[0] = 单帧 NV12 图像
pOutFrames[0] = 带框 BGR 图像
event.json = 检测结构化结果
```

---

### 5.5 销毁算法实例

```cpp
void ji_destroy_predictor(void *predictor);
```

作用：释放算法实例相关资源，包括模型、Dataset、NPU buffer、ACL context 等。

---

### 5.6 反初始化

```cpp
void ji_reinit();
```

当前 no-EVDeploy 版本中，该函数主要保留标准接口形式。

---

## 6. JiImageInfo 输入格式说明

`JiImageInfo` 定义如下：

```cpp
typedef struct {
    unsigned int    nWidth;
    unsigned int    nHeight;
    unsigned int    nWidthStride;
    unsigned int    nHeightStride;
    unsigned int    nFrameRate;
    unsigned long   dwTimeStamp;
    void*           pData;
    unsigned int    nDataLen;
    JiImageFormat   nFormat;
    JiDataType      nDataType;
    unsigned int    nFrameNo;
    unsigned char   byRes[4];
} JiImageInfo;
```

当前推荐输入格式：

```text
nFormat   = JI_IMAGE_TYPE_YUV420
nDataType = JI_UNSIGNED_CHAR
pData     = NV12 数据指针
```

对齐规则：

```cpp
aligned_width  = ALIGN_UP16(width);
aligned_height = ALIGN_UP2(height);
```

即：

```text
宽度按 16 对齐
高度按 2 对齐
NV12 数据大小 = aligned_width × aligned_height × 3 / 2
```

当前 `ji_calc_image` 内部会将 NV12 转 BGR，然后交给算法流程。

---

## 7. 输出结果说明

`ji_calc_image` 有两类输出。

### 7.1 输出图像

```cpp
JiImageInfo **pOutFrames;
unsigned int &nOutCount;
```

当前版本每次输出一张图：

```text
nOutCount = 1
pOutFrames[0] = 带检测框的 BGR 图
```

输出图格式：

```text
nFormat   = JI_IMAGE_TYPE_BGR
nDataType = JI_UNSIGNED_CHAR
nWidth    = 原图宽度
nHeight   = 原图高度
pData     = BGR 图像数据
```

注意：`pOutFrames[0].pData` 指向算法对象内部 `cv::Mat` 的数据缓冲区。外部调用方如果需要保存结果，应立即 copy，不要长期持有该指针。下一帧推理或销毁 predictor 后，该指针不应继续使用。

---

### 7.2 JiEvent 结构化结果

```cpp
typedef struct {
    JiEventTye code;
    const char * json;
} JiEvent;
```

事件状态：

```text
0   JISDK_CODE_ALARM   检测到目标
1   JISDK_CODE_NORMAL  未检测到目标
-1  JISDK_CODE_FAILED  推理失败
```

当前 JSON 输出示例：

```json
{
  "is_alert": true,
  "algorithm_data": {
    "is_alert": true,
    "target_info": [
      {
        "x": 578,
        "y": 270,
        "width": 622,
        "height": 432,
        "name": "excellent",
        "confidence": 0.812012
      }
    ]
  },
  "model_data": {
    "objects": [
      {
        "x": 578,
        "y": 270,
        "width": 622,
        "height": 432,
        "name": "excellent",
        "confidence": 0.812012
      }
    ]
  }
}
```

字段说明：

```text
is_alert                 是否检测到目标
algorithm_data           面向业务逻辑的检测结果
target_info              有效目标列表
model_data.objects       模型原始检测对象列表
x, y                     检测框左上角坐标
width, height            检测框宽高
name                     类别名：excellent / good
confidence               置信度
```

---

## 8. libji.so 本地测试命令

### 8.1 检查动态库导出接口

```bash
cd /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy
nm -D build/libji.so | grep " ji_"
```

### 8.2 最小加载测试

```bash
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om
export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:$LD_LIBRARY_PATH

python3 - <<'PY'
import ctypes

lib = ctypes.CDLL("/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/build/libji.so")

buf = ctypes.create_string_buffer(512)
ret = lib.ji_get_version(buf)
print("ji_get_version ret:", ret)
print("version:", buf.value.decode())

ret = lib.ji_init(0, None)
print("ji_init ret:", ret)

lib.ji_create_predictor.restype = ctypes.c_void_p
predictor = lib.ji_create_predictor(0)
print("predictor:", predictor)

if predictor:
    lib.ji_destroy_predictor(ctypes.c_void_p(predictor))
    print("destroy predictor OK")

lib.ji_reinit()
print("ji_reinit OK")
PY
```

---

## 9. HTTP API 服务说明

盒子端 HTTP 服务脚本：

```bash
/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/concrete_api_server.py
```

启动命令：

```bash
cd /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy

export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:$LD_LIBRARY_PATH
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om

python3 concrete_api_server.py
```

启动成功后，服务地址：

```text
http://192.168.1.2:8899
```

### 9.1 健康检查接口

```http
GET /health
```

浏览器访问：

```text
http://192.168.1.2:8899/health
```

当前成功返回：

```json
{
  "status": "ok",
  "model": "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om",
  "lib": "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/build/libji.so"
}
```

---

### 9.2 图片推理接口

```http
POST /infer
Content-Type: multipart/form-data
```

参数：

```text
file: 图片文件
```

Windows PowerShell 调用示例：

```powershell
curl.exe -X POST "http://192.168.1.2:8899/infer" `
  -F "file=@C:\Users\mb121\Desktop\test.jpg"
```

返回示例：

```json
{
  "ret": 0,
  "cost_ms": 90.31,
  "event_code": 0,
  "event": {
    "is_alert": true,
    "algorithm_data": {
      "is_alert": true,
      "target_info": [
        {
          "x": 578,
          "y": 270,
          "width": 622,
          "height": 432,
          "name": "excellent",
          "confidence": 0.812012
        }
      ]
    },
    "model_data": {
      "objects": []
    }
  },
  "result_url": "/result/result_xxxxx.jpg"
}
```

---

### 9.3 结果图访问接口

```http
GET /result/{name}
```

例如：

```text
http://192.168.1.2:8899/result/result_xxxxx.jpg
```

---

## 10. 电脑端图片调用方式

### 10.1 PowerShell 上传图片并下载结果

```powershell
$r = curl.exe -s -X POST "http://192.168.1.2:8899/infer" `
  -F "file=@C:\Users\mb121\Desktop\test.jpg" | ConvertFrom-Json

$r

Invoke-WebRequest `
  -Uri ("http://192.168.1.2:8899" + $r.result_url) `
  -OutFile "C:\Users\mb121\Desktop\concrete_result.jpg"
```

---

### 10.2 Python 调用图片推理

```python
import requests

server = "http://192.168.1.2:8899"
img_path = r"C:\Users\mb121\Desktop\test.jpg"

with open(img_path, "rb") as f:
    resp = requests.post(server + "/infer", files={"file": f})

print(resp.status_code)
data = resp.json()
print(data)

result_url = server + data["result_url"]
img = requests.get(result_url).content

with open(r"C:\Users\mb121\Desktop\concrete_result.jpg", "wb") as f:
    f.write(img)
```

---

## 11. 电脑端视频 HTTP 推理方式

电脑端脚本：

```text
D:\视频\pc_video_http_infer.py
```

功能：

```text
电脑本地读取视频
→ 每隔 stride 帧上传一帧到盒子 /infer
→ 获取 JSON 检测框
→ 电脑本地画框
→ 合成输出视频
```

### 11.1 安装电脑端依赖

```powershell
python -m pip install opencv-python requests tqdm -i https://pypi.tuna.tsinghua.edu.cn/simple
```

注意：不能安装 `cv2`，正确包名是 `opencv-python`。

验证：

```powershell
python -c "import cv2, requests, tqdm; print('cv2:', cv2.__version__)"
```

---

### 11.2 跑 5 秒快速测试

```powershell
cd D:\视频

python ".\pc_video_http_infer.py" `
  --server "http://192.168.1.2:8899" `
  --source "D:\视频\concrete\2025-10-21\AV4_20251021144151_20251021145459.mp4" `
  --out "D:\视频\concrete\2025-10-21\test_5s_http_result.mp4" `
  --seconds 5 `
  --stride 2
```

已完成的 5 秒测试结果：

```text
written frames: 125
infer count: 63
fail count: 0
elapsed: 10.92s
write fps: 11.45
infer fps: 5.77
```

说明：

```text
源视频约 25 FPS
5 秒 = 125 帧
stride=2，因此实际 HTTP 推理约 63 帧
fail count=0，说明 HTTP 调用稳定
```

---

### 11.3 跑 30 秒视频

```powershell
cd D:\视频

python ".\pc_video_http_infer.py" `
  --server "http://192.168.1.2:8899" `
  --source "D:\视频\concrete\2025-10-21\AV4_20251021144151_20251021145459.mp4" `
  --out "D:\视频\concrete\2025-10-21\AV4_20251021144151_30s_http_result.mp4" `
  --seconds 30 `
  --stride 2
```

参数解释：

```text
--server   盒子 HTTP API 地址
--source   电脑本地原始视频路径
--out      电脑本地输出视频路径
--seconds  处理前多少秒
--stride   每隔几帧调用一次盒子推理
```

建议：

```text
stride=1：每帧都推理，结果最精确，但速度较慢
stride=2：每 2 帧推理一次，速度更快，适合演示
stride=3/5：进一步加快，但框更新频率降低
```

---

## 12. 后台运行 HTTP 服务

如果不想一直占用终端，可在盒子上后台运行：

```bash
cd /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy

export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:$LD_LIBRARY_PATH
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om

nohup python3 concrete_api_server.py > /home/HwHiAiUser/evsdk_concrete_work/logs/concrete_api.log 2>&1 &
```

查看日志：

```bash
tail -f /home/HwHiAiUser/evsdk_concrete_work/logs/concrete_api.log
```

查看端口：

```bash
ss -lntp | grep 8899
```

停止服务：

```bash
ps -ef | grep concrete_api_server.py
kill <PID>
```

---

## 13. 常见问题

### 13.1 电脑打不开 `/health`

检查：

```text
1. 盒子 API 服务是否正在运行
2. 电脑是否能 ping 通 192.168.1.2
3. 盒子和电脑是否处在 Type-C / USB RNDIS 直连网络
4. 8899 端口是否被防火墙拦截
```

盒子上检查：

```bash
ss -lntp | grep 8899
```

---

### 13.2 `ModuleNotFoundError: No module named 'cv2'`

不要安装 `cv2`，执行：

```powershell
python -m pip install opencv-python requests tqdm -i https://pypi.tuna.tsinghua.edu.cn/simple
```

---

### 13.3 `ji_create_predictor failed`

通常是模型路径或 AscendCL 动态库路径问题。

检查：

```bash
ls -lh /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om

export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:$LD_LIBRARY_PATH
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om
```

---

### 13.4 HTTP 视频推理速度不够

可以增大 stride：

```powershell
--stride 3
```

或者：

```powershell
--stride 5
```

也可以降低上传 JPEG 质量：

```powershell
--jpeg-quality 70
```

---

### 13.5 为什么不用电脑直接推理

因为：

```text
libji.so 是 ARM64 + AscendCL 动态库
best.om 是 Ascend NPU 模型
电脑通常是 Windows x86_64，不能直接加载该库，也没有 Ascend NPU
```

因此正确方式是电脑通过 HTTP 请求调用盒子推理。

---

## 14. 当前版本交付说明

当前版本可以命名为：

```text
concrete_ev_sdk_ascendcl_v1
```

交付特征：

```text
1. 对外符合 EV_SDK ji.h 标准接口。
2. 内部不依赖 /usr/local/evdeploy。
3. 使用 AscendCL 直接加载 best.om。
4. 支持单图 ji_calc_image 推理。
5. 支持盒子本地视频逐帧推理。
6. 支持电脑通过 HTTP API 调用盒子完成图片/视频推理。
7. 已完成 5 秒 HTTP 视频测试，125 帧、63 次推理、0 失败。
```

当前核心成果：

```text
build/libji.so
model/concrete/best.om
concrete_api_server.py
pc_video_http_infer.py
data/concrete/ji_result.jpg
data/concrete/ji_video_result_100f.mp4
D:\视频\concrete\2025-10-21\test_5s_http_result.mp4
```
