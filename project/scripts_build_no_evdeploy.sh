#!/usr/bin/env bash
set -euo pipefail
cd /home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy
source /usr/local/Ascend/ascend-toolkit/set_env.sh
export CONCRETE_MODEL_PATH="${CONCRETE_MODEL_PATH:-/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om}"
rm -rf build
mkdir -p build
cd build
cmake .. -DWITH_ASCENDCL_RUNTIME=ON -DBUILD_EV_SDK_TESTS=OFF 2>&1 | tee /home/HwHiAiUser/evsdk_concrete_work/logs/cmake_no_evdeploy.log
make -j2 2>&1 | tee /home/HwHiAiUser/evsdk_concrete_work/logs/make_no_evdeploy.log
find . -maxdepth 3 -type f -name 'libji.so' -print
