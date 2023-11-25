#!/bin/bash
export CFLAGS="-O2 -march=i686 -pipe -static-libgcc -fstrict-aliasing -fcf-protection -ftree-vectorize -mstackrealign"
export LDFLAGS="-Wl,-O1 -static-libgcc -Wl,--dynamicbase -Wl,--nxcompat -Wl,--large-address-aware -Wl,--subsystem,console:5.01"
../configure --host=i686-w64-mingw32 \
	--enable-mini-gmp \
	--enable-fat \
	--enable-x86-aesni \
	--enable-x86-sha-ni \
	--enable-x86-pclmul
