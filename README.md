# concrete_ev_sdk

面向 **QA200A2-B / Atlas 200I A2 / Ascend 310B1** 的混凝土表面质量检测 EV_SDK/VAS 算法镜像与工程交付仓库。

本仓库用于交付混凝土质量检测算法在昇腾边缘盒子上的工程化部署结果。模型链路为：

```text
best.pt -> best.onnx -> best.om
```

当前正式交付版本为：

```text
Image: concrete_ev_sdk_atlas200_a2:1.0.1
Release: v1.0.1
Tar: concrete_ev_sdk_atlas200_a2_1.0.1.tar
SHA256: 4e0aa16950f7f9776b80d3311fed333215ba7c90247ccaf3740ee943a864df2b
```

> 注意：正式镜像 tar 约 1.2GB，不提交到 Git 仓库。请通过 GitHub Release 下载，或通过离线文件方式交付。Git 仓库只保留源码、构建脚本、交付文档、结构记录和 sha256 校验文件。

---

## 1. 项目说明

| 项目 | 内容 |
|---|---|
| 项目名称 | concrete_ev_sdk |
| 任务 | 混凝土表面质量检测 / 混凝土质量检测 |
| 类别 | `excellent`、`good` |
| 目标设备 | QA200A2-B / Atlas 200I A2 / Ascend 310B1 |
| 模型文件 | `/usr/local/ev_sdk/model/concrete/best.om` |
| 标签文件 | `/usr/local/ev_sdk/model/concrete/labels.txt` |
| 正式镜像 | `concrete_ev_sdk_atlas200_a2:1.0.1` |
| 接入方式 | VAS / EV_SDK 动态库加载 `libji.so` |
| 调试方式 | HTTP 服务仅用于裸机调试，不作为正式平台主接入方式 |

---

## 2. 当前交付状态

v1.0.1 已完成以下内容：

- 已基于轻量 `ascend-infer:v1.0.0` 构建正式算法镜像；
- 已对齐参考 EV_SDK/VAS 镜像的核心目录结构；
- 已放置 `libji.so`、`algo_config.json`、`best.om`、`labels.txt`；
- 已确认 EV_SDK 标准路径存在；
- 已确认 `libji.so` 导出核心接口；
- 已在挂载宿主机 Ascend driver 后完成 `ldd` 依赖检查，结果无 `not found`；
- 已导出 Docker 镜像 tar 与 sha256 校验文件。

本项目曾参考对方提供的：

```text
ehub.cvmart.net:8443/sdk/playphonehr_24760_atlas200_a2:1.0.28.2_test_sample
```

该镜像仅作为 EV_SDK/VAS 目录结构和调用方式参考，不作为最终镜像的 `FROM` 底座。最终交付镜像采用轻量底座，避免直接继承 9.87GB 参考镜像导致正式镜像体积过大。

---

## 3. GitHub Release 下载与校验

请在 Release `v1.0.1` 中下载：

```text
concrete_ev_sdk_atlas200_a2_1.0.1.tar
concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256
```

Windows PowerShell 校验：

```powershell
cd "G:\concrete\视频"
Get-FileHash .\concrete_ev_sdk_atlas200_a2_1.0.1.tar -Algorithm SHA256
Get-Content .\concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256
```

期望 SHA256：

```text
4e0aa16950f7f9776b80d3311fed333215ba7c90247ccaf3740ee943a864df2b
```

Linux 校验：

```bash
sha256sum concrete_ev_sdk_atlas200_a2_1.0.1.tar
cat concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256
```

---

## 4. 镜像加载

在 QA200A2-B / Atlas 200I A2 设备上执行：

```bash
docker load -i concrete_ev_sdk_atlas200_a2_1.0.1.tar
docker images | grep concrete_ev_sdk_atlas200_a2
```

期望镜像：

```text
concrete_ev_sdk_atlas200_a2:1.0.1
```

---

## 5. EV_SDK/VAS 标准路径

正式镜像内部关键路径如下：

```text
/usr/local/ev_sdk/
├── lib/
│   └── libji.so
├── config/
│   └── algo_config.json
├── model/
│   ├── algo_config.json
│   └── concrete/
│       ├── best.om
│       └── labels.txt
├── include/
│   ├── ji.h
│   ├── ji_error.h
│   ├── ji_types.h
│   └── ji_utils.h
└── test/
    ├── test_ji_calc_image.py
    └── test_ji_video.py
```

核心文件：

| 路径 | 说明 |
|---|---|
| `/usr/local/ev_sdk/lib/libji.so` | EV_SDK/VAS 动态库入口 |
| `/usr/local/ev_sdk/config/algo_config.json` | 平台配置路径 |
| `/usr/local/ev_sdk/model/algo_config.json` | 模型侧配置路径 |
| `/usr/local/ev_sdk/model/concrete/best.om` | Ascend OM 模型 |
| `/usr/local/ev_sdk/model/concrete/labels.txt` | 类别标签文件 |

---

## 6. algo_config.json 关键配置

```json
{
  "algorithm_name": "concrete_quality_detection",
  "algorithm_id": "concrete_ev_sdk",
  "model_path": "/usr/local/ev_sdk/model/concrete/best.om",
  "labels_path": "/usr/local/ev_sdk/model/concrete/labels.txt",
  "classes": ["excellent", "good"],
  "input_format": "NV12",
  "output_format": "BGR",
  "draw_roi_area": 1,
  "draw_result": 1,
  "show_result": 0,
  "gpu_id": 0,
  "threshold_value": 0.25,
  "score_threshold": 0.25,
  "conf_thresh": 0.25,
  "thresh": 0.25,
  "nms_threshold": 0.45,
  "language": "en"
}
```

---

## 7. libji.so 已确认导出接口

v1.0.1 已确认导出以下核心接口：

```text
ji_get_version
ji_create_predictor
ji_destroy_predictor
ji_calc_image
ji_calc_image_asyn
ji_update_config
```

可在构建宿主环境中使用如下命令检查：

```bash
nm -D build/libji.so | grep -E "ji_create_predictor|ji_calc_image|ji_calc_image_asyn|ji_destroy_predictor|ji_get_version|ji_update_config"
```

说明：

- 镜像内 `nm: not found` 是因为轻量运行镜像未内置 binutils，不代表 `libji.so` 异常；
- `ji_calc_frame` / `ji_calc_image_mix` 当前不是 v1.0.1 主路径接口；
- 如对方 VAS 平台实际强制调用 `ji_calc_frame` 或 `ji_calc_image_mix`，后续需要在 `libji.so` 中补 wrapper 接口。

---

## 8. 依赖检查

`libascend_hal.so` 由宿主机 Ascend Driver 提供，因此检查依赖时需要挂载：

```bash
docker run --rm --entrypoint /bin/sh \
  -v /usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64:ro \
  concrete_ev_sdk_atlas200_a2:1.0.1 \
  -lc 'ldd /usr/local/ev_sdk/lib/libji.so | grep -E "not found|libglog|libopencv|libascend_hal|libascendcl" || true'
```

已确认关键依赖可解析：

```text
libglog.so.0
libascendcl.so
libopencv_imgproc.so.4.5d
libopencv_core.so.4.5d
libascend_hal.so
```

期望结果：无 `not found`。

---

## 9. 仓库结构说明

```text
client/                     # 客户端或辅助调用代码
docs/                       # 文档、Release 校验文件、镜像结构记录
project/                    # EV_SDK/VAS 算法工程
README.md                   # 当前说明文档
README_DEPLOY.md            # 部署说明
MANIFEST.txt                # 文件清单
install_on_new_box.sh       # 新盒子安装辅助脚本
```

关键工程目录：

```text
project/
├── 3rd/wkt_parser
├── build
├── cmake
├── data/concrete
├── docker/sdk_image
├── include
├── model
├── src
├── CMakeLists.txt
├── concrete_api_server.py
├── scripts_build_no_evdeploy.sh
├── test_ji_calc_image.py
└── test_ji_video.py
```

---

## 10. 文档与记录

建议随仓库保留以下文件：

```text
docs/VAS_DOCKER_DELIVERY.md
docs/reference/playphonehr_reference_structure.txt
docs/reference/concrete_image_structure.txt
docs/release/concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256
```

其中：

- `playphonehr_reference_structure.txt`：对方参考 EV_SDK/VAS 镜像结构记录；
- `concrete_image_structure.txt`：本项目正式镜像结构记录；
- `concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256`：正式交付镜像校验文件；
- `VAS_DOCKER_DELIVERY.md`：面向对接方的正式交付说明。

---

## 11. Git 提交注意事项

不要把正式镜像 tar 提交进普通 Git 仓库：

```text
concrete_ev_sdk_atlas200_a2_1.0.1.tar
```

tar 文件应通过 GitHub Release 上传。Git 仓库只提交：

```bash
git add README.md docs/VAS_DOCKER_DELIVERY.md docs/reference docs/release
git commit -m "docs: add EV_SDK VAS delivery guide for v1.0.1"
git push origin main
```

---

## 12. 给对接方的简短说明

已完成混凝土质量检测算法 EV_SDK/VAS 镜像交付。

```text
GitHub: https://github.com/jiangchengyanyu/concrete_ev_sdk
Release: v1.0.1
Image tar: concrete_ev_sdk_atlas200_a2_1.0.1.tar
Image name: concrete_ev_sdk_atlas200_a2:1.0.1
Load command: docker load -i concrete_ev_sdk_atlas200_a2_1.0.1.tar
SHA256: 4e0aa16950f7f9776b80d3311fed333215ba7c90247ccaf3740ee943a864df2b
```

正式平台接入以 VAS/EV_SDK 加载 `/usr/local/ev_sdk/lib/libji.so` 为主，HTTP 服务仅用于裸机调试。
