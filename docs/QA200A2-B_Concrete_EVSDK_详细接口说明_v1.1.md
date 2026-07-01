# QA200A2-B 混凝土质量检测 EV_SDK 详细接口说明

> 版本：v1.1  
> 适用工程：`concrete_ev_sdk`  
> 适用设备：全爱科技 QA200A2-B / Atlas 200I A2 / Ascend 310B1 边缘 AI 盒子  
> 适用模型：YOLO11 混凝土表面质量检测模型，`best.om`  
> 适用部署方式：EV_SDK 标准 C 接口 `libji.so` + AscendCL 直接加载 OM 模型 + FastAPI HTTP 服务

---

## 1. 文档定位

本文档面向三类使用者：

1. **部署人员**：需要在新的 QA200A2-B 盒子上安装、启动、验证服务。
2. **算法/嵌入式开发人员**：需要理解 `libji.so` 的 EV_SDK 标准接口、输入图像格式、输出事件结构和内存约束。
3. **上位机/业务系统开发人员**：需要通过 HTTP API 调用盒子完成图片或视频帧推理。

当前工程已经完成以下能力验证：

- `best.om` 可以在 QA200A2-B / Ascend 310B1 上正常加载。
- `libji.so` 已成功封装 EV_SDK 标准 `ji.h` 接口。
- 当前版本不依赖 `/usr/local/evdeploy`，内部直接基于 AscendCL 调用 NPU。
- `ji_create_predictor` 可以加载模型并初始化 AscendCL。
- `ji_calc_image` 可以完成单帧真实推理，返回结构化 JSON 和带框结果图。
- FastAPI HTTP 服务可以通过 `/health`、`/infer`、`/result/{name}` 对外提供接口。
- Windows 电脑可以通过 HTTP 调用盒子完成图片推理和视频逐帧推理。

---

## 2. 系统总体接口分层

当前系统可以理解为 4 层接口：

```text
┌─────────────────────────────────────────────────────────────┐
│  第 4 层：业务调用接口                                      │
│  Windows / Web / 第三方系统 / 视频分析程序                  │
│  通过 HTTP 上传图片或视频帧                                 │
└─────────────────────────────────────────────────────────────┘
                         │ HTTP
                         ▼
┌─────────────────────────────────────────────────────────────┐
│  第 3 层：盒子 HTTP API                                     │
│  concrete_api_server.py                                     │
│  GET /health                                                │
│  POST /infer                                                │
│  GET /result/{name}                                         │
└─────────────────────────────────────────────────────────────┘
                         │ ctypes
                         ▼
┌─────────────────────────────────────────────────────────────┐
│  第 2 层：EV_SDK 标准 C 接口                                │
│  build/libji.so                                             │
│  ji_get_version / ji_init / ji_create_predictor / ji_calc_image │
└─────────────────────────────────────────────────────────────┘
                         │ C++ / AscendCL
                         ▼
┌─────────────────────────────────────────────────────────────┐
│  第 1 层：NPU 模型推理接口                                  │
│  AscendCL + best.om + Ascend 310B1                           │
│  输入：1×3×640×640 FP16                                     │
│  输出：1×6×8400 FP16                                        │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 各层职责

| 层级 | 模块 | 主要职责 | 使用者 |
|---|---|---|---|
| 第 4 层 | 上位机/业务系统 | 上传图片、解析 JSON、保存结果图、视频抽帧 | 电脑端、Web 端、业务系统 |
| 第 3 层 | HTTP API | 将外部 HTTP 请求转换为 `libji.so` 调用 | 应用开发人员 |
| 第 2 层 | EV_SDK C 接口 | 提供标准 `ji.h` 兼容接口 | SDK 集成、边缘盒子平台 |
| 第 1 层 | AscendCL 推理 | 加载 OM 模型，完成 NPU 推理和后处理 | 算法/底层开发 |

---

## 3. 设备、网络与端口说明

### 3.1 盒子设备信息

| 项目 | 当前值 |
|---|---|
| 设备 | QA200A2-B 后羿智盒 |
| NPU | Ascend 310B1 |
| 系统 | Ubuntu 22.04 LTS Arm64 |
| 主要推理后端 | AscendCL |
| Python | `/usr/local/miniconda3/bin/python` 或系统 `python3` |
| HTTP 服务端口 | `8899` |
| 默认 USB Type-C 盒子 IP | `192.168.1.2` |
| 建议电脑 USB 网卡 IP | `192.168.1.101/24` |

### 3.2 网络访问方式

电脑与盒子通过 Type-C / USB RNDIS 直连时，常见访问方式为：

```text
电脑 192.168.1.101  ───── USB Type-C / RNDIS ─────  盒子 192.168.1.2
```

盒子端服务地址：

```text
http://192.168.1.2:8899
```

本机回环访问：

```text
http://127.0.0.1:8899
```

### 3.3 网络连通性检查

电脑端检查：

```powershell
ping 192.168.1.2
curl.exe http://192.168.1.2:8899/health
```

盒子端检查：

```bash
ip -br a
ss -lntp | grep 8899
curl http://127.0.0.1:8899/health
```

如果电脑无法访问 `/health`，优先检查：

1. 盒子 HTTP 服务是否启动。
2. 盒子端口 `8899` 是否监听在 `0.0.0.0`。
3. 电脑 USB 网卡是否与 `192.168.1.2` 在同一网段。
4. Windows 防火墙或网络类型是否阻断。
5. 是否插错网口或 Type-C 数据链路未建立。

---

## 4. 仓库与部署目录说明

### 4.1 GitHub 仓库结构

推荐仓库结构如下：

```text
concrete_ev_sdk/
├── README.md
├── README_DEPLOY.md
├── install_on_new_box.sh
├── MANIFEST.txt
├── project/
│   ├── build/
│   │   └── libji.so
│   ├── model/
│   │   └── concrete/
│   │       ├── best.om
│   │       └── labels.txt
│   ├── concrete_api_server.py
│   ├── test_ji_calc_image.py
│   ├── test_ji_video.py
│   ├── CMakeLists.txt
│   ├── include/
│   ├── src/
│   ├── cmake/
│   └── 3rd/
├── client/
│   └── pc_video_http_infer.py
└── docs/
    ├── QA200A2-B_Concrete_EVSDK_详细接口说明.md
    ├── 接口与HTTP调用说明.md
    └── assets/
```

### 4.2 新盒子安装后的固定路径

当前 samepath 版本会把工程部署到：

```bash
/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy
```

核心路径：

```text
工程根目录：
/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy

模型路径：
/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om

动态库路径：
/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/build/libji.so

HTTP 服务脚本：
/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/concrete_api_server.py

日志目录：
/home/HwHiAiUser/evsdk_concrete_work/logs
```

### 4.3 一键安装命令

在新盒子上执行：

```bash
cd /home/HwHiAiUser
git clone https://github.com/jiangchengyanyu/concrete_ev_sdk.git
cd concrete_ev_sdk
bash install_on_new_box.sh
```

安装完成后检查：

```bash
systemctl status concrete-api --no-pager
curl http://127.0.0.1:8899/health
```

---

## 5. 模型接口说明

### 5.1 模型来源

当前模型部署链路：

```text
YOLO11 训练权重 best.pt
        │
        ▼
ONNX 导出 best.onnx
        │
        ▼
ATC 转换 best.om
        │
        ▼
Ascend 310B1 NPU 推理
```

### 5.2 类别定义

`model/concrete/labels.txt`：

```text
excellent
good
```

类别编号：

| 类别 ID | 类别名 | 业务含义 |
|---|---|---|
| 0 | `excellent` | 混凝土表面质量优秀 |
| 1 | `good` | 混凝土表面质量良好 |

### 5.3 模型输入

| 项目 | 值 |
|---|---|
| 输入张量 | `images` |
| Shape | `1 × 3 × 640 × 640` |
| Layout | `NCHW` |
| 数据类型 | `FP16` |
| 输入字节数 | `1 × 3 × 640 × 640 × 2 = 2457600 bytes` |
| 颜色空间 | RGB |
| 归一化 | `pixel / 255.0` |
| 前处理 | letterbox resize 到 640×640 |

### 5.4 模型输出

| 项目 | 值 |
|---|---|
| 输出 Shape | `1 × 6 × 8400` |
| 数据类型 | `FP16` |
| 输出字节数 | `1 × 6 × 8400 × 2 = 100800 bytes` |

输出通道含义：

| 通道 | 含义 |
|---|---|
| 0 | `x_center` |
| 1 | `y_center` |
| 2 | `width` |
| 3 | `height` |
| 4 | `excellent score` |
| 5 | `good score` |

注意：

```text
当前模型输出的类别分数已经是 0~1 概率值。
后处理阶段不需要再次执行 sigmoid。
```

### 5.5 后处理流程

后处理流程：

```text
读取 8400 个候选框
→ 取 excellent/good 中最大类别分数
→ 置信度阈值过滤
→ 坐标从 letterbox 尺度映射回原图
→ 对同类/所有目标执行 NMS
→ 得到最终检测框
→ 生成 JiEvent JSON
→ 在输出图上绘制检测框
```

当前已验证的单图结果示例：

```json
{
  "name": "excellent",
  "confidence": 0.812012,
  "x": 578,
  "y": 270,
  "width": 622,
  "height": 432
}
```

---

## 6. EV_SDK C 接口总览

当前 `build/libji.so` 对外导出 EV_SDK 标准 C 接口。

### 6.1 导出函数列表

可以在盒子上执行：

```bash
cd /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy
nm -D build/libji.so | grep " ji_"
```

当前导出函数包括：

```text
ji_get_version
ji_init
ji_reinit
ji_create_predictor
ji_destroy_predictor
ji_calc_image
ji_update_config
ji_set_callback
ji_calc_image_asyn
ji_create_face_db
ji_delete_face_db
ji_get_face_db_info
ji_face_add
ji_face_update
ji_face_delete
```

### 6.2 当前实际使用接口

| 接口 | 是否核心使用 | 说明 |
|---|---:|---|
| `ji_get_version` | 是 | 获取版本信息 |
| `ji_init` | 是 | 初始化 SDK 入口 |
| `ji_create_predictor` | 是 | 创建算法实例并加载模型 |
| `ji_calc_image` | 是 | 同步图片/单帧推理 |
| `ji_destroy_predictor` | 是 | 销毁算法实例 |
| `ji_reinit` | 是 | 反初始化入口 |
| `ji_update_config` | 保留 | 当前最小版基本忽略动态配置 |
| `ji_calc_image_asyn` | 保留 | 当前主要使用同步接口 |
| `ji_set_callback` | 保留 | 异步回调预留 |
| face db 系列接口 | 兼容保留 | 当前混凝土检测业务不使用 |

---

## 7. EV_SDK 核心接口详细说明

### 7.1 `ji_get_version`

#### 函数原型

```cpp
JiErrorCode ji_get_version(char *pVersion);
```

#### 功能

返回 SDK 版本和算法版本 JSON 字符串。

#### 参数

| 参数 | 方向 | 说明 |
|---|---|---|
| `pVersion` | 输出 | 调用方提供的字符缓冲区，函数将版本 JSON 写入该缓冲区 |

#### 返回值

| 返回值 | 说明 |
|---|---|
| `0` | 成功 |
| 非 0 | 失败 |

#### 返回示例

```json
{
  "sdk_version": "4.0.0",
  "algo_version": "1.0.1"
}
```

#### Python ctypes 示例

```python
import ctypes

lib = ctypes.CDLL("./build/libji.so")

buf = ctypes.create_string_buffer(512)
ret = lib.ji_get_version(buf)

print("ret:", ret)
print("version:", buf.value.decode("utf-8"))
```

---

### 7.2 `ji_init`

#### 函数原型

```cpp
JiErrorCode ji_init(int argc, char **argv);
```

#### 功能

初始化 SDK 框架入口。

当前 no-EVDeploy 版本中，`ji_init` 主要保留 EV_SDK 标准调用形式，不再初始化 `/usr/local/evdeploy`。真正的模型加载发生在 `ji_create_predictor`。

#### 参数

| 参数 | 方向 | 说明 |
|---|---|---|
| `argc` | 输入 | 参数数量，当前可传 `0` |
| `argv` | 输入 | 参数数组，当前可传 `nullptr` |

#### 返回值

| 返回值 | 说明 |
|---|---|
| `0` | 初始化成功 |
| 非 0 | 初始化失败 |

#### 推荐调用顺序

```text
ji_get_version
→ ji_init
→ ji_create_predictor
→ ji_calc_image
→ ji_destroy_predictor
→ ji_reinit
```

---

### 7.3 `ji_create_predictor`

#### 函数原型

```cpp
void* ji_create_predictor(JiPredictorType pdtype);
```

#### 功能

创建算法实例，并在内部加载 OM 模型。

内部主要执行：

```text
new SampleAlgorithm
→ SampleAlgorithm::Init
→ SampleDetector::Init
→ aclInit
→ aclrtSetDevice
→ aclrtCreateContext
→ aclmdlLoadFromFile(best.om)
→ 获取模型输入/输出大小
→ 创建输入/输出 dataset
→ 分配 device/host buffer
```

#### 参数

| 参数 | 方向 | 说明 |
|---|---|---|
| `pdtype` | 输入 | predictor 类型。当前混凝土检测版本可传 `0` |

#### 返回值

| 返回值 | 说明 |
|---|---|
| 非空指针 | 创建成功 |
| `nullptr` / `0` | 创建失败 |

#### 模型路径优先级

当前模型路径查找顺序建议按以下逻辑理解：

1. 环境变量 `CONCRETE_MODEL_PATH`
2. `/usr/local/ev_sdk/model/concrete/best.om`
3. `/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om`
4. 当前工作目录下的 `model/concrete/best.om`

推荐显式设置：

```bash
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om
```

#### 成功日志示例

```text
Concrete detector model path: .../model/concrete/best.om
model input size=2457600, output size=100800
Concrete detector init OK
Concrete SampleAlgorithm init OK
SamplePredictor init OK
```

#### 常见失败原因

| 现象 | 可能原因 | 处理方法 |
|---|---|---|
| 返回空指针 | `best.om` 不存在 | 检查模型路径 |
| ACL 初始化失败 | Ascend 环境变量未加载 | `source /usr/local/Ascend/ascend-toolkit/set_env.sh` |
| 找不到 `libascendcl.so` | `LD_LIBRARY_PATH` 缺失 | 添加 Ascend lib64 路径 |
| 模型加载失败 | OM 与设备/版本不匹配 | 确认 OM 是 Ascend 310B1 可用版本 |

---

### 7.4 `ji_calc_image`

#### 函数原型

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

#### 功能

同步单帧推理接口。它是当前系统最核心的检测入口。

完整流程：

```text
接收 JiImageInfo 输入帧
→ 校验输入数量和数据指针
→ NV12 转 BGR
→ letterbox resize 到 640×640
→ BGR 转 RGB
→ HWC 转 CHW
→ FP16 归一化
→ H2D 拷贝
→ aclmdlExecute
→ D2H 拷贝
→ 解析 1×6×8400 输出
→ 置信度过滤
→ NMS
→ 坐标映射回原图
→ 绘制结果图
→ 生成 JiEvent JSON
```

#### 参数说明

| 参数 | 方向 | 说明 |
|---|---|---|
| `predictor` | 输入 | `ji_create_predictor` 返回的算法实例 |
| `pInFrames` | 输入 | 输入帧数组指针 |
| `nInCount` | 输入 | 输入帧数量，当前应为 `1` |
| `args` | 输入 | 额外参数 JSON 字符串，当前版本可传空字符串或 `nullptr` |
| `pOutFrames` | 输出 | 输出帧数组指针的地址 |
| `nOutCount` | 输出 | 输出帧数量，当前成功时通常为 `1` |
| `event` | 输出 | 事件结构，包含状态码和 JSON 字符串 |

#### 输入约束

| 项目 | 当前要求 |
|---|---|
| 输入数量 | `nInCount = 1` |
| 输入格式 | 推荐 `JI_IMAGE_TYPE_YUV420` |
| 数据类型 | `JI_UNSIGNED_CHAR` |
| 图像内容 | NV12 数据 |
| 宽度对齐 | `ALIGN_UP16(width)` |
| 高度对齐 | `ALIGN_UP2(height)` |
| 数据长度 | `aligned_width × aligned_height × 3 / 2` |

#### 输出约束

| 项目 | 当前结果 |
|---|---|
| `nOutCount` | 成功时为 `1` |
| 输出图格式 | BGR |
| 输出图尺寸 | 原图宽高 |
| 事件 JSON | 检测结果结构化字段 |
| 检测到目标 | `event.code = 0` |
| 未检测到目标 | `event.code = 1` |
| 失败 | `event.code = -1` |

#### 注意事项

1. `predictor` 必须是有效指针，不能在销毁后继续使用。
2. 当前版本按单实例同步调用设计，不建议多个线程同时调用同一个 predictor。
3. `pOutFrames[0].pData` 指向算法对象内部缓存，调用方需要立即复制。
4. `event.json` 也建议调用方立即复制为字符串，不要长期持有内部指针。
5. 如果外部业务系统是多路摄像头，建议每一路单独建立 predictor，或在 HTTP 层做请求队列。

---

### 7.5 `ji_destroy_predictor`

#### 函数原型

```cpp
void ji_destroy_predictor(void *predictor);
```

#### 功能

销毁算法实例并释放资源。

释放内容包括：

```text
SampleAlgorithm
SampleDetector
ACL model desc
ACL model id
input dataset
output dataset
host/device buffer
ACL context
```

#### 调用时机

推荐在程序退出或模型服务停止时调用。

```text
服务启动：ji_create_predictor
服务运行：重复调用 ji_calc_image
服务停止：ji_destroy_predictor
```

不要每次请求都创建和销毁 predictor，否则会导致模型重复加载，明显降低性能。

---

### 7.6 `ji_reinit`

#### 函数原型

```cpp
void ji_reinit();
```

#### 功能

保留 EV_SDK 标准反初始化接口。

当前版本中主要用于 SDK 生命周期闭环。一般在全部 predictor 销毁后调用。

---

### 7.7 `ji_update_config`

#### 函数原型

```cpp
JiErrorCode ji_update_config(void* predictor, const char* args);
```

> 如果你当前头文件中的函数签名略有不同，以仓库 `include/ji.h` 为准。

#### 当前状态

当前混凝土检测最小可用版本主要保留该接口，暂未将阈值、NMS、类别过滤等参数完整开放为动态配置。

#### 后续建议支持的配置项

```json
{
  "conf_threshold": 0.25,
  "nms_threshold": 0.45,
  "max_detections": 100,
  "draw_result": true,
  "target_classes": ["excellent", "good"]
}
```

建议后续实现后，通过该接口动态更新：

| 字段 | 类型 | 说明 |
|---|---|---|
| `conf_threshold` | number | 置信度阈值 |
| `nms_threshold` | number | NMS 阈值 |
| `max_detections` | integer | 最大输出目标数 |
| `draw_result` | boolean | 是否绘制检测框 |
| `target_classes` | array | 只输出指定类别 |

---

## 8. JiImageInfo 结构详细说明

### 8.1 结构定义

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

### 8.2 字段说明

| 字段 | 类型 | 方向 | 说明 |
|---|---|---|---|
| `nWidth` | `unsigned int` | 输入/输出 | 原始图像宽度 |
| `nHeight` | `unsigned int` | 输入/输出 | 原始图像高度 |
| `nWidthStride` | `unsigned int` | 输入/输出 | 图像行 stride，NV12 输入建议 16 对齐 |
| `nHeightStride` | `unsigned int` | 输入/输出 | 图像高 stride，NV12 输入建议 2 对齐 |
| `nFrameRate` | `unsigned int` | 输入 | 帧率，可填源视频 FPS |
| `dwTimeStamp` | `unsigned long` | 输入 | 时间戳，单位由业务侧约定 |
| `pData` | `void*` | 输入/输出 | 图像数据指针 |
| `nDataLen` | `unsigned int` | 输入/输出 | 图像数据长度 |
| `nFormat` | `JiImageFormat` | 输入/输出 | 图像格式 |
| `nDataType` | `JiDataType` | 输入/输出 | 数据类型 |
| `nFrameNo` | `unsigned int` | 输入 | 帧号 |
| `byRes` | `unsigned char[4]` | 保留 | 保留字段 |

### 8.3 NV12 输入数据要求

当前 `ji_calc_image` 推荐输入：

```text
nFormat   = JI_IMAGE_TYPE_YUV420
nDataType = JI_UNSIGNED_CHAR
pData     = NV12 buffer
```

对齐规则：

```cpp
aligned_width  = (width  + 15) / 16 * 16;
aligned_height = (height + 1)  / 2  * 2;
```

数据长度：

```cpp
nDataLen = aligned_width * aligned_height * 3 / 2;
```

示例：原图 `1280 × 720`

```text
aligned_width  = 1280
aligned_height = 720
nDataLen       = 1280 × 720 × 3 / 2 = 1382400 bytes
```

示例：原图 `1279 × 719`

```text
aligned_width  = 1280
aligned_height = 720
nDataLen       = 1280 × 720 × 3 / 2 = 1382400 bytes
```

### 8.4 输出 BGR 图像

当前输出帧：

```text
nFormat   = JI_IMAGE_TYPE_BGR
nDataType = JI_UNSIGNED_CHAR
nWidth    = 原图宽度
nHeight   = 原图高度
nDataLen  = width × height × 3
pData     = BGR 数据指针
```

输出图可直接用 OpenCV 保存：

```cpp
cv::Mat out_bgr(height, width, CV_8UC3, pOutFrames[0].pData);
cv::imwrite("result.jpg", out_bgr);
```

---

## 9. JiEvent 结构详细说明

### 9.1 结构定义

```cpp
typedef struct {
    JiEventTye code;
    const char * json;
} JiEvent;
```

### 9.2 事件状态码

| code | 名称 | 含义 | HTTP 层对应语义 |
|---:|---|---|---|
| `0` | `JISDK_CODE_ALARM` | 检测到目标 | 成功，有目标 |
| `1` | `JISDK_CODE_NORMAL` | 未检测到目标 | 成功，无目标 |
| `-1` | `JISDK_CODE_FAILED` | 推理失败 | 请求失败或内部错误 |

### 9.3 JSON 顶层结构

```json
{
  "is_alert": true,
  "algorithm_data": {
    "is_alert": true,
    "target_info": []
  },
  "model_data": {
    "objects": []
  }
}
```

字段说明：

| 字段 | 类型 | 说明 |
|---|---|---|
| `is_alert` | boolean | 是否检测到目标 |
| `algorithm_data` | object | 面向业务平台的检测结果 |
| `algorithm_data.is_alert` | boolean | 业务侧是否报警 |
| `algorithm_data.target_info` | array | 业务侧目标列表 |
| `model_data` | object | 模型侧原始/近原始结果 |
| `model_data.objects` | array | 模型检测对象列表 |

### 9.4 单个目标字段

```json
{
  "x": 578,
  "y": 270,
  "width": 622,
  "height": 432,
  "name": "excellent",
  "confidence": 0.812012
}
```

字段说明：

| 字段 | 类型 | 单位 | 说明 |
|---|---|---|---|
| `x` | integer | pixel | 检测框左上角 x 坐标 |
| `y` | integer | pixel | 检测框左上角 y 坐标 |
| `width` | integer | pixel | 检测框宽度 |
| `height` | integer | pixel | 检测框高度 |
| `name` | string | - | 类别名，`excellent` 或 `good` |
| `confidence` | number | - | 置信度，范围 0~1 |

### 9.5 坐标系说明

图像坐标系采用 OpenCV 常用坐标系：

```text
(0, 0) --------------------> x
  |
  |
  |
  v
  y
```

检测框含义：

```text
x, y：左上角坐标
width, height：框宽高
右下角坐标：
x2 = x + width
y2 = y + height
```

### 9.6 有目标返回示例

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

### 9.7 无目标返回示例

```json
{
  "is_alert": false,
  "algorithm_data": {
    "is_alert": false,
    "target_info": []
  },
  "model_data": {
    "objects": []
  }
}
```

### 9.8 失败返回建议格式

如果推理失败，建议 HTTP 层统一返回：

```json
{
  "ret": -1,
  "cost_ms": 0,
  "event_code": -1,
  "event": {
    "is_alert": false,
    "algorithm_data": {
      "is_alert": false,
      "target_info": []
    },
    "model_data": {
      "objects": []
    }
  },
  "error": "ji_calc_image failed",
  "result_url": null
}
```

---

## 10. HTTP API 详细说明

盒子端 HTTP 服务由 `concrete_api_server.py` 提供。

### 10.1 服务启动

手动启动：

```bash
cd /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy

export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:$LD_LIBRARY_PATH
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om

python3 concrete_api_server.py
```

后台服务启动：

```bash
systemctl restart concrete-api
systemctl status concrete-api --no-pager
```

查看日志：

```bash
journalctl -u concrete-api -n 100 --no-pager
journalctl -u concrete-api -f
```

### 10.2 API 总览

| 方法 | 路径 | 功能 | 输入 | 输出 |
|---|---|---|---|---|
| GET | `/health` | 健康检查 | 无 | 服务状态、模型路径、库路径 |
| POST | `/infer` | 图片推理 | multipart 图片文件 | JSON 结果 + 结果图 URL |
| GET | `/result/{name}` | 访问结果图 | 图片名 | JPEG/PNG 图片文件 |

---

### 10.3 `GET /health`

#### 功能

检查 HTTP 服务是否运行，模型路径和动态库路径是否配置正确。

#### 请求

```http
GET /health HTTP/1.1
Host: 192.168.1.2:8899
```

#### curl 示例

```bash
curl http://127.0.0.1:8899/health
```

电脑端：

```powershell
curl.exe http://192.168.1.2:8899/health
```

#### 成功响应

```json
{
  "status": "ok",
  "model": "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om",
  "lib": "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/build/libji.so"
}
```

#### 字段说明

| 字段 | 类型 | 说明 |
|---|---|---|
| `status` | string | 服务状态，成功为 `ok` |
| `model` | string | 当前使用的 OM 模型路径 |
| `lib` | string | 当前加载的 `libji.so` 路径 |

#### 排查意义

| 现象 | 说明 |
|---|---|
| `/health` 无法访问 | HTTP 服务未启动或网络不通 |
| `status=ok` | 服务进程存在，但仍建议再测 `/infer` |
| `model` 路径不对 | 检查 `CONCRETE_MODEL_PATH` |
| `lib` 路径不对 | 检查 `concrete_api_server.py` 中动态库路径 |

---

### 10.4 `POST /infer`

#### 功能

上传一张图片到盒子，由盒子调用 `libji.so -> ji_calc_image -> AscendCL -> best.om` 完成推理，并返回检测结果 JSON。

#### 请求格式

```http
POST /infer HTTP/1.1
Host: 192.168.1.2:8899
Content-Type: multipart/form-data
```

表单字段：

| 字段名 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `file` | File | 是 | 待检测图片 |

支持图片格式：

```text
以 OpenCV 能成功解码为准，通常包括 jpg / jpeg / png / bmp。
```

推荐：

```text
jpg / jpeg
```

#### Windows PowerShell 示例

```powershell
curl.exe -X POST "http://192.168.1.2:8899/infer" `
  -F "file=@C:\Users\mb121\Desktop\test.jpg"
```

#### Linux/macOS curl 示例

```bash
curl -X POST "http://192.168.1.2:8899/infer" \
  -F "file=@/path/to/test.jpg"
```

#### Python requests 示例

```python
import requests

server = "http://192.168.1.2:8899"
image_path = r"C:\Users\mb121\Desktop\test.jpg"

with open(image_path, "rb") as f:
    resp = requests.post(
        server + "/infer",
        files={"file": ("test.jpg", f, "image/jpeg")},
        timeout=30,
    )

print(resp.status_code)
print(resp.json())
```

#### 成功响应示例

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
  },
  "result_url": "/result/result_xxxxx.jpg"
}
```

#### 响应字段说明

| 字段 | 类型 | 说明 |
|---|---|---|
| `ret` | integer | `ji_calc_image` 返回值，`0` 表示成功 |
| `cost_ms` | number | 单次 HTTP 推理耗时，单位 ms |
| `event_code` | integer | EV_SDK 事件码，`0` 有目标，`1` 无目标，`-1` 失败 |
| `event` | object | 检测事件 JSON |
| `result_url` | string | 带框结果图的访问路径 |

#### 结果图下载

PowerShell：

```powershell
$r = curl.exe -s -X POST "http://192.168.1.2:8899/infer" `
  -F "file=@C:\Users\mb121\Desktop\test.jpg" | ConvertFrom-Json

Invoke-WebRequest `
  -Uri ("http://192.168.1.2:8899" + $r.result_url) `
  -OutFile "C:\Users\mb121\Desktop\concrete_result.jpg"
```

Python：

```python
import requests

server = "http://192.168.1.2:8899"

with open(r"C:\Users\mb121\Desktop\test.jpg", "rb") as f:
    data = requests.post(server + "/infer", files={"file": f}, timeout=30).json()

result_url = server + data["result_url"]
img_bytes = requests.get(result_url, timeout=30).content

with open(r"C:\Users\mb121\Desktop\concrete_result.jpg", "wb") as f:
    f.write(img_bytes)
```

#### HTTP 状态码建议

| 状态码 | 场景 |
|---:|---|
| 200 | 请求成功，推理完成 |
| 400 | 未上传文件或文件格式错误 |
| 500 | 服务内部错误、`libji.so` 调用失败、NPU 推理失败 |

---

### 10.5 `GET /result/{name}`

#### 功能

访问 `/infer` 生成的带框结果图。

#### 请求示例

```text
http://192.168.1.2:8899/result/result_xxxxx.jpg
```

#### curl 下载

```bash
curl -o result.jpg http://192.168.1.2:8899/result/result_xxxxx.jpg
```

#### PowerShell 下载

```powershell
Invoke-WebRequest `
  -Uri "http://192.168.1.2:8899/result/result_xxxxx.jpg" `
  -OutFile ".\result.jpg"
```

#### 注意事项

1. `result_url` 是相对路径，需要拼接服务器地址。
2. 结果图是否长期保存取决于 `concrete_api_server.py` 当前实现。
3. 如果用于生产系统，建议增加定期清理结果图机制，避免磁盘占满。

---

## 11. HTTP 调用时序

### 11.1 服务启动时序

```text
systemctl start concrete-api
        │
        ▼
加载 Python FastAPI 服务
        │
        ▼
ctypes 加载 build/libji.so
        │
        ▼
ji_init
        │
        ▼
ji_create_predictor
        │
        ▼
aclInit / aclrtSetDevice / aclmdlLoadFromFile
        │
        ▼
服务开始监听 0.0.0.0:8899
```

### 11.2 单次 `/infer` 调用时序

```text
客户端上传图片
        │
        ▼
FastAPI 接收 multipart file
        │
        ▼
OpenCV 解码图片
        │
        ▼
BGR 转 NV12 / 构造 JiImageInfo
        │
        ▼
调用 ji_calc_image
        │
        ▼
AscendCL NPU 推理
        │
        ▼
后处理 + NMS + 画框
        │
        ▼
返回 JiEvent JSON 和 BGR 输出图
        │
        ▼
HTTP 返回 JSON
        │
        ▼
客户端按 result_url 下载结果图
```

---

## 12. 电脑端视频接口说明

### 12.1 视频推理策略

当前电脑端视频推理不是直接上传整个视频，而是：

```text
电脑读取视频
→ 按帧抽取
→ 每隔 stride 帧上传一张图片
→ 盒子返回检测框 JSON
→ 电脑端在原视频帧上画框
→ 写出新视频
```

这样设计的原因：

1. 盒子 HTTP API 当前以单图推理为核心，接口简单稳定。
2. 视频文件通常较大，直接上传整个视频不利于实时演示。
3. 电脑端负责视频解码/编码，盒子专注 NPU 推理。
4. 通过 `stride` 可以灵活平衡速度和检测频率。

### 12.2 脚本位置

仓库内：

```text
client/pc_video_http_infer.py
```

本地常用路径：

```text
D:\视频\pc_video_http_infer.py
```

### 12.3 电脑端依赖

```powershell
python -m pip install opencv-python requests tqdm -i https://pypi.tuna.tsinghua.edu.cn/simple
```

验证：

```powershell
python -c "import cv2, requests, tqdm; print('cv2:', cv2.__version__)"
```

### 12.4 参数说明

| 参数 | 必填 | 说明 |
|---|---:|---|
| `--server` | 是 | 盒子 HTTP 服务地址，例如 `http://192.168.1.2:8899` |
| `--source` | 是 | 电脑本地输入视频路径 |
| `--out` | 是 | 电脑本地输出视频路径 |
| `--seconds` | 否 | 只处理前 N 秒，适合快速测试 |
| `--stride` | 否 | 每隔多少帧调用一次推理 |
| `--jpeg-quality` | 否 | 上传 JPEG 压缩质量，如果脚本支持该参数，可用于控制速度 |

### 12.5 5 秒快速测试

```powershell
cd D:\视频

python ".\pc_video_http_infer.py" `
  --server "http://192.168.1.2:8899" `
  --source "D:\视频\concrete\2025-10-21\AV4_20251021144151_20251021145459.mp4" `
  --out "D:\视频\concrete\2025-10-21\test_5s_http_result.mp4" `
  --seconds 5 `
  --stride 2
```

已验证结果：

```text
written frames: 125
infer count: 63
fail count: 0
elapsed: 10.92s
write fps: 11.45
infer fps: 5.77
```

### 12.6 30 秒演示视频

```powershell
cd D:\视频

python ".\pc_video_http_infer.py" `
  --server "http://192.168.1.2:8899" `
  --source "D:\视频\concrete\2025-10-21\AV4_20251021144151_20251021145459.mp4" `
  --out "D:\视频\concrete\2025-10-21\AV4_20251021144151_30s_http_result.mp4" `
  --seconds 30 `
  --stride 2
```

### 12.7 stride 选择建议

| stride | 含义 | 优点 | 缺点 | 推荐场景 |
|---:|---|---|---|---|
| 1 | 每帧推理 | 框最连续 | 最慢 | 离线高质量结果 |
| 2 | 每 2 帧推理 | 速度和效果平衡 | 少量跳帧 | 演示推荐 |
| 3 | 每 3 帧推理 | 更快 | 框更新频率下降 | 长视频快速预览 |
| 5 | 每 5 帧推理 | 很快 | 框可能不连续 | 快速筛查 |

---

## 13. systemd 服务接口说明

### 13.1 服务名称

```text
concrete-api
```

### 13.2 常用命令

启动：

```bash
systemctl start concrete-api
```

停止：

```bash
systemctl stop concrete-api
```

重启：

```bash
systemctl restart concrete-api
```

查看状态：

```bash
systemctl status concrete-api --no-pager
```

开机自启：

```bash
systemctl enable concrete-api
```

取消自启：

```bash
systemctl disable concrete-api
```

查看日志：

```bash
journalctl -u concrete-api -n 100 --no-pager
journalctl -u concrete-api -f
```

### 13.3 服务状态判断

| 现象 | 说明 |
|---|---|
| `active (running)` | 服务正在运行 |
| `failed` | 启动失败，查看 `journalctl` |
| `/health` 返回 `ok` | HTTP 服务可访问 |
| `/infer` 成功返回 JSON | 模型和 `libji.so` 可用 |

---

## 14. 环境变量接口说明

### 14.1 `LD_LIBRARY_PATH`

用途：让系统找到 AscendCL 动态库，例如 `libascendcl.so`。

推荐设置：

```bash
export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:$LD_LIBRARY_PATH
```

如果设备使用 CANN 7.0.RC1 路径，也可能是：

```bash
export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/7.0.RC1/aarch64-linux/lib64:$LD_LIBRARY_PATH
```

### 14.2 `CONCRETE_MODEL_PATH`

用途：指定当前混凝土检测 OM 模型路径。

推荐设置：

```bash
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om
```

### 14.3 Ascend set_env

推荐在手动调试时执行：

```bash
source /usr/local/Ascend/ascend-toolkit/set_env.sh
```

如果该路径不存在，检查：

```bash
ls /usr/local/Ascend/ascend-toolkit/
find /usr/local/Ascend -name set_env.sh
```

---

## 15. 本地调试与自检命令

### 15.1 检查 NPU

```bash
npu-smi info
```

重点关注：

```text
Health
Power
Temp
Memory Usage
```

### 15.2 检查模型文件

```bash
ls -lh /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om
ls -lh /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/labels.txt
```

### 15.3 检查动态库

```bash
ls -lh /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/build/libji.so
ldd /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/build/libji.so
```

如果看到 `not found`，说明动态库依赖缺失，需要检查 `LD_LIBRARY_PATH` 或安装依赖。

### 15.4 检查导出符号

```bash
nm -D /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/build/libji.so | grep " ji_"
```

至少应看到：

```text
ji_get_version
ji_init
ji_create_predictor
ji_destroy_predictor
ji_calc_image
ji_reinit
```

### 15.5 最小 ctypes 加载测试

```bash
cd /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy

export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:$LD_LIBRARY_PATH
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om

python3 - <<'PY'
import ctypes

lib_path = "./build/libji.so"
lib = ctypes.CDLL(lib_path)

buf = ctypes.create_string_buffer(512)
ret = lib.ji_get_version(buf)
print("ji_get_version ret:", ret)
print("version:", buf.value.decode("utf-8"))

ret = lib.ji_init(0, None)
print("ji_init ret:", ret)

lib.ji_create_predictor.restype = ctypes.c_void_p
predictor = lib.ji_create_predictor(0)
print("predictor:", predictor)

if predictor:
    lib.ji_destroy_predictor(ctypes.c_void_p(predictor))
    print("ji_destroy_predictor OK")

lib.ji_reinit()
print("ji_reinit OK")
PY
```

### 15.6 单图真实推理测试

```bash
cd /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy

export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:$LD_LIBRARY_PATH
export CONCRETE_MODEL_PATH=/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om

python3 test_ji_calc_image.py
```

预期结果：

```text
ji_calc_image_ret: 0
out_count: 1
event_code: 0
detected: 1
class: excellent
confidence: 0.812...
```

---

## 16. 错误码与故障排查

### 16.1 HTTP 层问题

| 问题 | 表现 | 处理 |
|---|---|---|
| 服务没启动 | `/health` 连接失败 | `systemctl status concrete-api` |
| 端口没监听 | `Connection refused` | `ss -lntp | grep 8899` |
| 网络不通 | `ping 192.168.1.2` 失败 | 检查 USB 网卡 IP |
| 上传字段错误 | `/infer` 报 400 | 表单字段必须叫 `file` |
| 图片解码失败 | `/infer` 报文件格式错误 | 换 jpg/png，确认图片能打开 |

### 16.2 libji.so 层问题

| 问题 | 表现 | 处理 |
|---|---|---|
| 找不到 `libji.so` | `ctypes.CDLL` 失败 | 检查 `project/build/libji.so` |
| 找不到 Ascend 动态库 | `libascendcl.so not found` | 设置 `LD_LIBRARY_PATH` |
| 模型加载失败 | `ji_create_predictor` 返回空 | 检查 `CONCRETE_MODEL_PATH` |
| 输入格式错误 | `ji_calc_image` 返回失败 | 检查 NV12、stride、data len |
| 输出空结果 | `event_code=1` | 可能没有目标或阈值过高 |

### 16.3 AscendCL/NPU 层问题

| 问题 | 表现 | 处理 |
|---|---|---|
| NPU 不可见 | `npu-smi info` 异常 | 重启盒子或检查驱动 |
| ACL 初始化失败 | `aclInit failed` | source CANN 环境 |
| 模型不兼容 | `aclmdlLoadFromFile failed` | 确认 OM 转换目标芯片 |
| 显存不足 | 加载模型失败 | `npu-smi info` 查看占用 |

### 16.4 GitHub 部署层问题

| 问题 | 表现 | 处理 |
|---|---|---|
| clone 失败 | 无法访问 GitHub | 换网络或用代理/手动上传 zip |
| 模型缺失 | `best.om` 不存在 | 确认仓库是否完整推送 |
| 脚本权限问题 | `Permission denied` | `chmod +x install_on_new_box.sh` |
| apt 时间校验问题 | `Release file is not valid yet` | 同步系统时间或临时关闭日期检查 |

---

## 17. 性能与并发说明

### 17.1 当前已验证性能

电脑端 5 秒视频 HTTP 推理测试结果：

```text
源视频帧率：约 25 FPS
处理帧数：125
stride：2
实际推理次数：63
失败次数：0
总耗时：10.92 s
写出 FPS：11.45
推理 FPS：5.77
```

### 17.2 性能组成

单次 `/infer` 耗时包括：

```text
HTTP 上传耗时
+ 图片解码耗时
+ BGR/NV12 格式转换耗时
+ ji_calc_image 调用耗时
+ NPU 推理耗时
+ 后处理/NMS耗时
+ 结果图保存耗时
+ HTTP 返回耗时
```

### 17.3 并发建议

当前版本建议：

```text
1. 单服务实例
2. 单 predictor 常驻内存
3. HTTP 层串行或低并发调用
4. 不建议多个请求同时抢同一个 predictor
```

生产部署如需多路并发，建议：

1. 在 HTTP 服务层增加队列和互斥锁。
2. 每个摄像头通道建立独立 predictor。
3. 评估 NPU 显存是否允许多个模型实例。
4. 增加请求超时和失败重试。
5. 将 `/infer` 和视频流处理解耦为异步任务队列。

---

## 18. 安全与生产化建议

当前版本是工程验证和演示版本，若进入生产环境，建议补充：

### 18.1 访问控制

- 增加 token 或 API key。
- 限制访问 IP。
- 不直接暴露到公网。
- 使用 Nginx 反向代理时增加上传大小限制。

### 18.2 文件管理

- 限制上传文件大小。
- 限制上传文件类型。
- 定期清理 `/result/` 结果图。
- 对文件名做随机化，避免路径穿越。

### 18.3 服务稳定性

- systemd 自动重启。
- 日志轮转。
- 健康检查脚本。
- 异常请求保护。
- NPU 状态定时检查。

### 18.4 接口增强

建议后续增加：

| 接口 | 功能 |
|---|---|
| `GET /version` | 返回 SDK、算法、模型版本 |
| `GET /metrics` | 返回累计请求数、平均耗时、失败次数 |
| `POST /infer_json` | 支持 base64 图片 JSON 输入 |
| `POST /config` | 动态调整阈值、NMS、类别过滤 |
| `GET /config` | 查看当前推理配置 |
| `POST /reload_model` | 重新加载模型 |
| `POST /infer_video` | 盒子端直接处理短视频 |

---

## 19. 第三方系统对接建议

### 19.1 最小对接流程

第三方系统只需要实现：

```text
1. 检查盒子在线：GET /health
2. 上传图片：POST /infer
3. 解析 event.algorithm_data.target_info
4. 根据 result_url 下载结果图
```

### 19.2 业务系统只关心的字段

一般只需要解析：

```json
{
  "event_code": 0,
  "event": {
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
    }
  }
}
```

### 19.3 Python 最小对接函数

```python
import requests

def infer_concrete(server: str, image_path: str, timeout: int = 30) -> dict:
    with open(image_path, "rb") as f:
        resp = requests.post(
            server.rstrip("/") + "/infer",
            files={"file": f},
            timeout=timeout,
        )
    resp.raise_for_status()
    return resp.json()

if __name__ == "__main__":
    data = infer_concrete("http://192.168.1.2:8899", "test.jpg")
    targets = data["event"]["algorithm_data"]["target_info"]
    for obj in targets:
        print(obj["name"], obj["confidence"], obj["x"], obj["y"], obj["width"], obj["height"])
```

### 19.4 C# / Java / Web 对接

只要能发送 `multipart/form-data` 请求即可，不依赖 Python。

接口协议本质是：

```text
POST http://盒子IP:8899/infer
Content-Type: multipart/form-data
字段名：file
字段值：图片二进制文件
```

---

## 20. 推荐 README 中保留的接口摘要

如果 README 不想太长，可以只保留：

```text
GET /health
POST /infer
GET /result/{name}
```

详细接口说明放在：

```text
docs/QA200A2-B_Concrete_EVSDK_详细接口说明.md
```

README 中引用：

```markdown
详细接口见：[QA200A2-B_Concrete_EVSDK_详细接口说明](docs/QA200A2-B_Concrete_EVSDK_详细接口说明.md)
```

---

## 21. 当前版本边界

当前版本已经适合：

- 单图推理演示。
- 电脑端视频逐帧 HTTP 推理。
- QA200A2-B 新盒子同路径快速部署。
- EV_SDK 标准接口验证。
- GitHub 工程交付。

当前版本暂不等价于完整生产平台，以下能力建议后续增强：

- 多路摄像头并发。
- 用户鉴权。
- 动态阈值配置。
- 盒子端视频流直接接入。
- RTSP 实时流服务。
- 自动日志轮转和磁盘清理。
- 模型热更新。
- 接口版本管理。

---

## 22. 快速命令汇总

### 新盒子部署

```bash
cd /home/HwHiAiUser
git clone https://github.com/jiangchengyanyu/concrete_ev_sdk.git
cd concrete_ev_sdk
bash install_on_new_box.sh
```

### 服务检查

```bash
systemctl status concrete-api --no-pager
curl http://127.0.0.1:8899/health
```

### 电脑端检查

```powershell
curl.exe http://192.168.1.2:8899/health
```

### 电脑端图片推理

```powershell
curl.exe -X POST "http://192.168.1.2:8899/infer" `
  -F "file=@D:\视频\test.jpg"
```

### 下载结果图

```powershell
$r = curl.exe -s -X POST "http://192.168.1.2:8899/infer" `
  -F "file=@D:\视频\test.jpg" | ConvertFrom-Json

Invoke-WebRequest `
  -Uri ("http://192.168.1.2:8899" + $r.result_url) `
  -OutFile "D:\视频\concrete_result.jpg"
```

### 电脑端视频推理

```powershell
python ".\client\pc_video_http_infer.py" `
  --server "http://192.168.1.2:8899" `
  --source "D:\视频\concrete\2025-10-21\AV4_20251021144151_20251021145459.mp4" `
  --out "D:\视频\concrete\2025-10-21\http_result.mp4" `
  --seconds 30 `
  --stride 2
```

---

## 23. 版本记录

| 版本 | 日期 | 说明 |
|---|---|---|
| v1.0 | 2026-07-01 | 完成基础接口和 HTTP 调用说明 |
| v1.1 | 2026-07-01 | 扩展为详细开发者接口文档，补充 C 接口、HTTP API、结构体、事件 JSON、systemd、环境变量、调试与故障排查 |
