#!/bin/sh
make clean
make distclean
./configure --prefix=/media/huangch/D/workspace/tcga-image-pipeline/openremoteslide-341.0.1 LDFLAGS='-lcurl'
make all
make install
