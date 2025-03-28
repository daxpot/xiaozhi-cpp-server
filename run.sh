#!/bin/bash
# 如果没有传入参数 $1，则默认使用 web_server
if [ -z "$1" ]; then
    TARGET="web_server"
else
    TARGET="$1"
fi

cd build || { echo "Error: Cannot enter build directory"; exit 1; }
cmake ..

TARGET_NAME=$(basename "$TARGET")

make "$TARGET_NAME" || { echo "Error: Make failed"; exit 1; }
cd ..
./build/apps/"$TARGET" || { echo "Error: Failed to run $TARGET_NAME"; exit 1; }