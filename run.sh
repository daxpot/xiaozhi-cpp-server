#!/bin/bash
# 先运行scripts/install_deps.sh安装依赖
# 如果没有传入参数 $1，则默认使用 web_server
if [ -z "$1" ]; then
    TARGET="web_server"
else
    TARGET="$1"
fi

git submodule update --init --recursive
mkdir -p build
cd build || { echo "Error: Cannot enter build directory"; exit 1; }
cmake -DKALDI_NATIVE_FBANK_BUILD_TESTS=OFF -DKALDI_NATIVE_FBANK_BUILD_PYTHON=OFF ..

TARGET_NAME=$(basename "$TARGET")

make "$TARGET_NAME" || { echo "Error: Make failed"; exit 1; }
cd ..
./build/apps/"$TARGET" || { echo "Error: Failed to run $TARGET_NAME"; exit 1; }