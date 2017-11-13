#!/bin/sh
rm -rf /Volumes/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1/bin
rm -rf /Volumes/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1/include
rm -rf /Volumes/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1/lib
rm -rf /Volumes/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1/share
./configure --prefix=/cygdrive/d/workspace/tcga-image-pipeline/openremoteslide-341.0.1 --host=x86_64-w64-mingw32 --build=x86_64-unknown-cygwin --disable-static --disable-dependency-tracking PKG_CONFIG="pkg-config" PKG_CONFIG_LIBDIR="/cygdrive/c/openslide-winbuild/64/root/lib/pkgconfig" PKG_CONFIG_PATH="" CC="x86_64-w64-mingw32-gcc -static-libgcc" CPPFLAGS="-D_FORTIFY_SOURCE=2 -I/cygdrive/c/openslide-winbuild/64/root/include" CFLAGS="-O2 -g -mms-bitfields -fexceptions" CXXFLAGS="-O2 -g -mms-bitfields -fexceptions" LDFLAGS="-static-libgcc -Wl,--enable-auto-image-base -Wl,--dynamicbase -Wl,--nxcompat -lcurl -L/cygdrive/c/openslide-winbuild/64/root/lib"
make
make install
