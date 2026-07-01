# Concrete EV_SDK Same-Path Deployment

## 说明

本包用于 QA200A2-B / Ascend 310B 盒子快速部署混凝土质量检测服务。

特点：

- 已包含成熟版 `libji.so`
- 已包含 `best.om`
- 不依赖 `/usr/local/evdeploy`
- 内部使用 AscendCL 加载 OM 模型
- 对外提供 HTTP API：`http://盒子IP:8899/infer`

## 安装

```bash
tar -xzf concrete_ev_sdk_samepath_v1_xxx.tar.gz -C /tmp
cd /tmp/concrete_ev_sdk_samepath_v1
bash install_on_new_box.sh
systemctl restart concrete-api
```

## 健康检查

盒子本机：

```bash
curl http://127.0.0.1:8899/health
```

电脑浏览器：

```text
http://192.168.1.2:8899/health
```

## 图片推理

```bash
curl -X POST "http://192.168.1.2:8899/infer" -F "file=@test.jpg"
```

## 常用命令

```bash
systemctl restart concrete-api
systemctl status concrete-api --no-pager
journalctl -u concrete-api -f
```
