# EV_SDK / VAS Docker 镜像交付说明

本文档说明 `concrete_ev_sdk_atlas200_a2:1.0.1` 在 QA200A2-B / Atlas 200I A2 / Ascend 310B1 上的 EV_SDK/VAS 交付内容、路径约定、加载方式和验证方法。

---

## 1. 交付版本

| 项目 | 内容 |
|---|---|
| 项目名称 | concrete_ev_sdk |
| 算法任务 | 混凝土表面质量检测 |
| 目标设备 | QA200A2-B / Atlas 200I A2 / Ascend 310B1 |
| 推理后端 | Ascend 310B1 / OM 模型 |
| 镜像名称 | `concrete_ev_sdk_atlas200_a2:1.0.1` |
| 镜像文件 | `concrete_ev_sdk_atlas200_a2_1.0.1.tar` |
| 镜像大小 | 约 1.2GB / 1184MB |
| SHA256 | `4e0aa16950f7f9776b80d3311fed333215ba7c90247ccaf3740ee943a864df2b` |
| 正式接入方式 | VAS / EV_SDK 加载 `libji.so` |
| 调试接入方式 | HTTP API，仅用于裸机调试 |

---

## 2. 设计决策说明

对方提供的 `playphonehr_24760_atlas200_a2:1.0.28.2_test_sample` 镜像已作为 EV_SDK/VAS 标准参考镜像进行结构对齐。该参考镜像体积约 9.87GB，直接继承会导致正式交付镜像过大。

因此，本项目最终采用：

```text
ascend-infer:v1.0.0
```

作为轻量运行底座，并按照参考镜像的 EV_SDK/VAS 标准路径放置以下内容：

```text
/usr/local/ev_sdk/lib/libji.so
/usr/local/ev_sdk/config/algo_config.json
/usr/local/ev_sdk/model/algo_config.json
/usr/local/ev_sdk/model/concrete/best.om
/usr/local/ev_sdk/model/concrete/labels.txt
```

该策略保证：

1. 对 VAS/EV_SDK 平台暴露标准动态库入口；
2. 保持模型、配置、标签路径稳定；
3. 避免继承参考镜像中不属于本算法的示例工程；
4. 显著降低正式交付镜像体积。

---

## 3. 镜像加载

在目标设备上执行：

```bash
docker load -i concrete_ev_sdk_atlas200_a2_1.0.1.tar
```

加载后确认：

```bash
docker images | grep concrete_ev_sdk_atlas200_a2
```

期望出现：

```text
concrete_ev_sdk_atlas200_a2   1.0.1
```

---

## 4. SHA256 校验

Linux：

```bash
sha256sum concrete_ev_sdk_atlas200_a2_1.0.1.tar
cat concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256
```

Windows PowerShell：

```powershell
Get-FileHash .\concrete_ev_sdk_atlas200_a2_1.0.1.tar -Algorithm SHA256
Get-Content .\concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256
```

期望值：

```text
4e0aa16950f7f9776b80d3311fed333215ba7c90247ccaf3740ee943a864df2b
```

---

## 5. 镜像内部标准路径

```text
/usr/local/ev_sdk/
├── lib/
│   ├── libji.so
│   ├── libglog.so.0
│   ├── libopencv_core.so.4.5d
│   └── libopencv_imgproc.so.4.5d
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

---

## 6. 必需文件检查

可使用以下命令检查镜像内关键文件：

```bash
docker run --rm --entrypoint /bin/sh concrete_ev_sdk_atlas200_a2:1.0.1 -lc '
set -e
ls -lh /usr/local/ev_sdk/lib/libji.so
ls -lh /usr/local/ev_sdk/config/algo_config.json
ls -lh /usr/local/ev_sdk/model/algo_config.json
ls -lh /usr/local/ev_sdk/model/concrete/best.om
ls -lh /usr/local/ev_sdk/model/concrete/labels.txt
ls -lh /usr/local/ev_sdk/include/ji.h
'
```

---

## 7. VAS / EV_SDK 动态库接口

v1.0.1 已确认导出以下接口：

```text
ji_get_version
ji_create_predictor
ji_destroy_predictor
ji_calc_image
ji_calc_image_asyn
ji_update_config
```

构建环境检查命令：

```bash
nm -D build/libji.so | grep -E "ji_create_predictor|ji_calc_image|ji_calc_image_asyn|ji_destroy_predictor|ji_get_version|ji_update_config"
```

说明：

- 运行镜像内未安装 `nm` 属于正常情况；
- `ji_calc_frame` / `ji_calc_image_mix` 当前不是 v1.0.1 主路径接口；
- 如对接平台要求固定调用 `ji_calc_frame` 或 `ji_calc_image_mix`，需要在后续版本补充 wrapper。

---

## 8. 依赖检查

由于 `libascend_hal.so` 由宿主机驱动提供，检查 `libji.so` 依赖时需要挂载宿主机 driver：

```bash
docker run --rm --entrypoint /bin/sh \
  -v /usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64:ro \
  concrete_ev_sdk_atlas200_a2:1.0.1 \
  -lc 'ldd /usr/local/ev_sdk/lib/libji.so | grep -E "not found|libglog|libopencv|libascend_hal|libascendcl" || true'
```

已验证解析到的关键依赖包括：

```text
libglog.so.0 => /usr/local/ev_sdk/lib/libglog.so.0
libascendcl.so => /usr/local/Ascend/nnrt/latest/lib64/libascendcl.so
libopencv_imgproc.so.4.5d => /usr/local/ev_sdk/lib/libopencv_imgproc.so.4.5d
libopencv_core.so.4.5d => /usr/local/ev_sdk/lib/libopencv_core.so.4.5d
libascend_hal.so => /usr/local/Ascend/driver/lib64/libascend_hal.so
```

期望结果：无 `not found`。

---

## 9. algo_config.json

配置文件同时放置在：

```text
/usr/local/ev_sdk/config/algo_config.json
/usr/local/ev_sdk/model/algo_config.json
```

关键字段如下：

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

## 10. 交付清单

建议 Git 仓库提交：

```text
README.md
docs/VAS_DOCKER_DELIVERY.md
docs/reference/playphonehr_reference_structure.txt
docs/reference/concrete_image_structure.txt
docs/release/concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256
```

建议 GitHub Release 上传：

```text
concrete_ev_sdk_atlas200_a2_1.0.1.tar
concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256
```

不要将 1.2GB tar 文件提交到普通 Git 仓库。

---

## 11. GitHub Release 操作

### 方式一：网页上传

1. 打开 GitHub 仓库；
2. 进入 `Releases`；
3. 点击 `Draft a new release`；
4. Tag 填写：`v1.0.1`；
5. Title 填写：`concrete_ev_sdk_atlas200_a2 v1.0.1`；
6. 上传：
   - `concrete_ev_sdk_atlas200_a2_1.0.1.tar`
   - `concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256`
7. 发布 Release。

### 方式二：GitHub CLI

首次发布：

```powershell
gh release create v1.0.1 `
  "G:\concrete\视频\concrete_ev_sdk_atlas200_a2_1.0.1.tar" `
  "G:\concrete\视频\concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256" `
  --repo jiangchengyanyu/concrete_ev_sdk `
  --title "concrete_ev_sdk_atlas200_a2 v1.0.1" `
  --notes "正式 EV_SDK/VAS 算法镜像，已完成路径、符号和 ldd 依赖检查。"
```

如果 Release 已存在，只补传资产：

```powershell
gh release upload v1.0.1 `
  "G:\concrete\视频\concrete_ev_sdk_atlas200_a2_1.0.1.tar" `
  "G:\concrete\视频\concrete_ev_sdk_atlas200_a2_1.0.1.tar.sha256" `
  --repo jiangchengyanyu/concrete_ev_sdk `
  --clobber
```

---

## 12. 对接方确认口径

可发送给对接方：

```text
已完成混凝土质量检测算法 EV_SDK/VAS 镜像交付。

GitHub: https://github.com/jiangchengyanyu/concrete_ev_sdk
Release: v1.0.1
Image tar: concrete_ev_sdk_atlas200_a2_1.0.1.tar
Image name: concrete_ev_sdk_atlas200_a2:1.0.1
Load command: docker load -i concrete_ev_sdk_atlas200_a2_1.0.1.tar
SHA256: 4e0aa16950f7f9776b80d3311fed333215ba7c90247ccaf3740ee943a864df2b

核心路径：
/usr/local/ev_sdk/lib/libji.so
/usr/local/ev_sdk/config/algo_config.json
/usr/local/ev_sdk/model/algo_config.json
/usr/local/ev_sdk/model/concrete/best.om
/usr/local/ev_sdk/model/concrete/labels.txt

已验证：
1. EV_SDK 标准路径存在；
2. libji.so 导出 ji_get_version、ji_create_predictor、ji_destroy_predictor、ji_calc_image、ji_calc_image_asyn、ji_update_config；
3. 挂载 /usr/local/Ascend/driver/lib64 后 ldd 无 not found；
4. 镜像结构已参考 playphonehr_24760_atlas200_a2:1.0.28.2_test_sample 对齐；
5. HTTP 服务仅用于裸机调试，正式平台接入以 VAS/EV_SDK 加载 libji.so 为主。
```
