#!/bin/sh
rm -rf /Volumes/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1/bin
rm -rf /Volumes/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1/include
rm -rf /Volumes/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1/lib
rm -rf /Volumes/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1/share
make clean
make distclean
./configure --prefix=/Volumes/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1 CFLAGS='-I/usr/local/Cellar/openjpeg/1.5.2_1/include/openjpeg-1.5' LDFLAGS='-L/opt/local/lib -lcurl'
make
make install
