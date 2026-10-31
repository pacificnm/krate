#!/bin/sh
# Builds the Krate .deb (packaging/README.md): a clean release build in
# build-deb/, then the package in dist/. Install it with
#   sudo apt install ./dist/krate_*.deb
set -eu
cd "$(dirname "$0")/.."
cmake -S . -B build-deb -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-deb
ctest --test-dir build-deb --output-on-failure -j"$(nproc)"
mkdir -p dist
cpack --config build-deb/CPackConfig.cmake -B dist
rm -rf dist/_CPack_Packages
ls -l dist/*.deb
