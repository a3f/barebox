#!/bin/sh

set -exu

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
cp mnt-reform-defconfig .config
make -j $(nproc)
