#!/bin/sh
RET=0
mkdir "${TRAVIS_BUILD_DIR}/build"
cd "${TRAVIS_BUILD_DIR}/build"
cmake --version
cmake .. \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_BUILD_TYPE=Release \
	-DENABLE_NLS=OFF \
	-DBUILD_TESTING=ON \
	|| exit 1
# Build everything.
make -k || RET=1
# Test with en_US.UTF8.
LC_ALL="en_US.UTF8" ctest -V || RET=1
# Test with fr_FR.UTF8 to find i18n issues.
LC_ALL="fr_FR.UTF8" ctest -V || RET=1
exit "${RET}"
