# Docker SDK 镜像说明

本文档说明混凝土质量检测算法在 QA200A2-B / Atlas 200I A2 / Ascend 310B1 盒子上的 Docker SDK 镜像制作方式。

当前镜像用于对接 CVMart / 极市 VAS 平台。平台最终运行时会通过 VAS 流媒体框架加载 EV_SDK 标准算法库，因此本项目的 Docker 镜像重点不是启动 HTTP 服务，而是提供符合平台路径要求的 `libji.so`、模型文件和算法配置文件。

---

## 1. 镜像内部目录结构

镜像构建完成后，关键文件会放置在以下路径：

```text
/usr/local/ev_sdk/lib/libji.so
/usr/local/ev_sdk/model/algo_config.json
/usr/local/ev_sdk/config/algo_config.json
/usr/local/ev_sdk/model/concrete/best.om
/usr/local/ev_sdk/model/concrete/labels.txt
```

其中：

```text
/usr/local/ev_sdk/lib/libji.so
```

是平台 VAS 程序加载的 EV_SDK 算法动态库。

```text
/usr/local/ev_sdk/model/concrete/best.om
```

是混凝土质量检测模型文件。

```text
/usr/local/ev_sdk/model/concrete/labels.txt
```

是类别标签文件。

```text
/usr/local/ev_sdk/model/algo_config.json
/usr/local/ev_sdk/config/algo_config.json
```

是算法配置文件。由于平台模板中同时出现过 `model/algo_config.json` 和 `config/algo_config.json` 两种路径，为提高兼容性，本镜像中两个位置均放置同一份配置文件。

---

## 2. 当前 local 验证版镜像

当前已经在盒子上构建完成的 local 验证版镜像为：

```text
concrete_ev_sdk_atlas200_a2:1.0.1-local
```

已导出的镜像文件为：

```text
concrete_ev_sdk_atlas200_a2_1.0.1-local.tar
```

该镜像基于盒子本地已有的基础镜像构建：

```text
ascend-infer:v1.0.0
```

该版本主要用于本地验证和备份。正式交付给平台时，建议在获得 CVMart SDK 基础镜像权限后重新构建正式版。

---

## 3. local 版构建命令

在项目根目录执行：

```bash
docker build \
  -f project/docker/sdk_image/Dockerfile \
  --build-arg BASE_IMAGE=ascend-infer:v1.0.0 \
  -t concrete_ev_sdk_atlas200_a2:1.0.1-local \
  project
```

如果当前已经进入 `project` 目录，也可以执行：

```bash
docker build \
  -f docker/sdk_image/Dockerfile \
  --build-arg BASE_IMAGE=ascend-infer:v1.0.0 \
  -t concrete_ev_sdk_atlas200_a2:1.0.1-local \
  .
```

---

## 4. 正式版构建命令

正式交付版建议基于 CVMart / 极市提供的 SDK 基础镜像重新构建：

```text
ehub.cvmart.net:8443/sdk/playphonehr_24760_atlas200_a2:1.0.28.2
```

构建命令如下：

```bash
docker build \
  -f project/docker/sdk_image/Dockerfile \
  --build-arg BASE_IMAGE=ehub.cvmart.net:8443/sdk/playphonehr_24760_atlas200_a2:1.0.28.2 \
  -t concrete_ev_sdk_atlas200_a2:1.0.1 \
  project
```

如果当前已经进入 `project` 目录，也可以执行：

```bash
docker build \
  -f docker/sdk_image/Dockerfile \
  --build-arg BASE_IMAGE=ehub.cvmart.net:8443/sdk/playphonehr_24760_atlas200_a2:1.0.28.2 \
  -t concrete_ev_sdk_atlas200_a2:1.0.1 \
  .
```

目前该 SDK 基础镜像需要仓库权限。如果 `docker pull` 出现 `unauthorized`，需要向平台方申请 `ehub.cvmart.net:8443` 的登录账号、token，或要求平台方提供该 SDK 镜像的离线 tar 包。

---

## 5. 已打包的运行依赖

为避免平台加载 `libji.so` 时出现动态库缺失，本镜像已将以下依赖库打包到：

```text
/usr/local/ev_sdk/lib
```

包含：

```text
libglog.so.0
libgflags.so.2.2
libopencv_core.so.4.5d
libopencv_imgproc.so.4.5d
libtbb.so.2
```

这些依赖用于支持 `libji.so` 中的日志、OpenCV 图像处理和线程运行时。

---

## 6. Ascend Driver 运行依赖

`libascend_hal.so` 不直接打包进镜像。该库属于 Ascend Driver 运行时库，应由最终 VAS 容器启动时通过宿主机挂载提供。

平台最终启动容器时应挂载：

```text
/usr/local/Ascend/driver/lib64
```

示例：

```bash
-v /usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64:ro
```

当前镜像中的 `LD_LIBRARY_PATH` 已包含：

```text
/usr/local/ev_sdk/lib
/usr/local/Ascend/driver/lib64
/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64
/usr/local/Ascend/ascend-toolkit/latest/lib64
```

因此在挂载 Ascend Driver 后，`libji.so` 的关键依赖可以正常解析。

---

## 7. 镜像内容验证命令

可以使用以下命令检查镜像内部文件：

```bash
docker run --rm --entrypoint /bin/sh concrete_ev_sdk_atlas200_a2:1.0.1-local -lc '
echo "===== files ====="
ls -lh /usr/local/ev_sdk/lib/libji.so
ls -lh /usr/local/ev_sdk/model/algo_config.json
ls -lh /usr/local/ev_sdk/config/algo_config.json
ls -lh /usr/local/ev_sdk/model/concrete/best.om
ls -lh /usr/local/ev_sdk/model/concrete/labels.txt

echo "===== algo_config ====="
cat /usr/local/ev_sdk/model/algo_config.json

echo "===== ldd libji.so ====="
ldd /usr/local/ev_sdk/lib/libji.so || true
'
```

带 Ascend Driver 挂载的依赖验证命令：

```bash
docker run --rm --entrypoint /bin/sh \
  -v /usr/local/Ascend/driver/lib64:/usr/local/Ascend/driver/lib64:ro \
  concrete_ev_sdk_atlas200_a2:1.0.1-local \
  -lc '
echo "===== LD_LIBRARY_PATH ====="
echo "$LD_LIBRARY_PATH"

echo "===== check driver lib ====="
ls -lh /usr/local/Ascend/driver/lib64/libascend_hal.so || true

echo "===== ldd libji.so with driver mount ====="
ldd /usr/local/ev_sdk/lib/libji.so | grep -E "not found|libglog|libopencv|libascend_hal|libascendcl" || true
'
```

如果输出中没有 `not found`，说明镜像依赖解析正常。

---

## 8. 镜像导出命令

local 版镜像导出命令：

```bash
docker save concrete_ev_sdk_atlas200_a2:1.0.1-local \
  -o concrete_ev_sdk_atlas200_a2_1.0.1-local.tar
```

正式版镜像导出命令：

```bash
docker save concrete_ev_sdk_atlas200_a2:1.0.1 \
  -o concrete_ev_sdk_atlas200_a2_1.0.1.tar
```

注意：导出的 `.tar` 镜像文件通常较大，不建议提交到 GitHub 仓库。

---

## 9. 与 VAS 平台的关系

本项目 Docker SDK 镜像只负责提供算法运行所需文件：

```text
libji.so
best.om
labels.txt
algo_config.json
运行依赖库
```

平台方后续会基于该算法 SDK 镜像进行二次封装，将 VAS 流媒体框架、`camera_list.conf`、`local.conf`、`run.conf` 等运行配置打入最终镜像或在启动时挂载。

最终平台运行时通常会挂载：

```text
/usr/local/vas/camera_list.conf
/usr/local/vas/local.conf
/usr/local/vas/run.conf
/usr/local/vas/vas_data/pic
/usr/local/vas/vas_data/log
```

因此本项目不在 Docker SDK 镜像中启动 `concrete_api_server.py`。该 HTTP 服务仅用于裸机调试和 PC 端调用测试，正式平台接入以 EV_SDK `libji.so` 为主。

---

## 10. 当前状态

当前已完成：

```text
1. 本地 EV_SDK 算法库 libji.so 构建；
2. Ascend OM 模型 best.om 打包；
3. labels.txt 打包；
4. algo_config.json 同时放置到 model 和 config 两个路径；
5. glog、gflags、OpenCV、TBB 运行依赖打包；
6. local 版 Docker 镜像构建；
7. 挂载 Ascend Driver 后 ldd 依赖检查通过；
8. 镜像 tar 和工程备份包已导出。
```

当前推荐保留版本：

```text
concrete_ev_sdk_atlas200_a2:1.0.1-local
```

正式交付版本应在获得 CVMart SDK 基础镜像权限后重新构建：

```text
concrete_ev_sdk_atlas200_a2:1.0.1
```
