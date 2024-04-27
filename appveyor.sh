#!/bin/sh
set -ev
cmake --version

mkdir build || true
cd build

cmake .. -G Ninja \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_BUILD_TYPE=Release \
	-DENABLE_NLS=OFF \
	-DBUILD_TESTING=ON
