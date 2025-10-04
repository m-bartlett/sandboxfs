#!/usr/bin/env sh
set -o errexit

cmake $@ -S . -B ./build --fresh
cmake --build  ./build  --target all -j$(( $(nproc) - 1 ))
sudo cmake --install ./build