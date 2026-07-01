#!/bin/bash
set -e

TARGET="/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy"
PKG_DIR=$(cd "$(dirname "$0")" && pwd)

echo "==== Concrete EV_SDK Same-Path Installer ===="
echo "package: ${PKG_DIR}"
echo "target : ${TARGET}"

if [ "$(id -u)" != "0" ]; then
    echo "ERROR: please run as root"
    exit 1
fi

echo "[1/7] prepare apt source"
cp /etc/apt/sources.list /etc/apt/sources.list.bak.$(date +%F_%H%M%S) 2>/dev/null || true
cat > /etc/apt/sources.list <<'SRC'
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ jammy main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ jammy-updates main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ jammy-backports main restricted universe multiverse
deb https://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ jammy-security main restricted universe multiverse
SRC
apt clean || true
apt -o Acquire::Check-Date=false -o Acquire::Check-Valid-Until=false update || true

echo "[2/7] install dependencies"
apt -o Acquire::Check-Date=false -o Acquire::Check-Valid-Until=false install -y \
    python3-pip \
    libopencv-dev \
    libgoogle-glog-dev \
    pkg-config || true

python3 -m pip config set global.index-url https://pypi.tuna.tsinghua.edu.cn/simple || true
python3 -m pip install fastapi uvicorn python-multipart requests tqdm || true

echo "[3/7] copy project files"
mkdir -p "${TARGET}"
cp -r "${PKG_DIR}/project/"* "${TARGET}/"

echo "[4/7] check Ascend and libji"
source /usr/local/Ascend/ascend-toolkit/set_env.sh 2>/dev/null || true
export LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:/usr/local/Ascend/ascend-toolkit/latest/lib64:${LD_LIBRARY_PATH}
export CONCRETE_MODEL_PATH="${TARGET}/model/concrete/best.om"
ldd "${TARGET}/build/libji.so" || true

echo "[5/7] smoke test ji_get_version / ji_create_predictor"
python3 - <<PY
import ctypes, os
lib_path = "${TARGET}/build/libji.so"
os.environ["CONCRETE_MODEL_PATH"] = "${TARGET}/model/concrete/best.om"
lib = ctypes.CDLL(lib_path)
buf = ctypes.create_string_buffer(512)
print("ji_get_version ret:", lib.ji_get_version(buf))
print("version:", buf.value.decode())
print("ji_init ret:", lib.ji_init(0, None))
lib.ji_create_predictor.restype = ctypes.c_void_p
p = lib.ji_create_predictor(0)
print("predictor:", p)
if p:
    lib.ji_destroy_predictor(ctypes.c_void_p(p))
    print("destroy predictor OK")
lib.ji_reinit()
print("smoke test OK")
PY

echo "[6/7] create systemd service"
PYTHON_BIN="/usr/local/miniconda3/bin/python"
if [ ! -x "$PYTHON_BIN" ]; then
  PYTHON_BIN=$(command -v python3)
fi

cat > /etc/systemd/system/concrete-api.service <<SERVICE
[Unit]
Description=Concrete Quality Detection API
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=${TARGET}
Environment=CONCRETE_MODEL_PATH=${TARGET}/model/concrete/best.om
Environment=LD_LIBRARY_PATH=/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64:/usr/local/Ascend/ascend-toolkit/latest/lib64
ExecStart=${PYTHON_BIN} ${TARGET}/concrete_api_server.py
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
SERVICE

systemctl daemon-reload
systemctl enable concrete-api.service

echo "[7/7] done"
echo
echo "Start:"
echo "  systemctl restart concrete-api"
echo
echo "Check:"
echo "  systemctl status concrete-api --no-pager"
echo "  curl http://127.0.0.1:8899/health"
