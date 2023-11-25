#!/bin/bash
export CFLAGS="-O2 -pipe -static-libgcc -fstrict-aliasing -fcf-protection -ftree-vectorize"
export LDFLAGS="-Wl,-O1 -static-libgcc -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -Wl,--subsystem,console:5.02"
../configure --host=x86_64-w64-mingw32 \
	--enable-mini-gmp \
	--enable-fat \
	--enable-x86-aesni \
	--enable-x86-sha-ni \
	--enable-x86-pclmul
