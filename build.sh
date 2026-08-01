#!/bin/sh

set -exu

if [ -z ${SOURCE_DATE_EPOCH:+x} ] && git -C . rev-parse 2>/dev/null; then
	SOURCE_DATE_EPOCH=$(git log -1 --format=%ct)
	export SOURCE_DATE_EPOCH
fi
export KBUILD_BUILD_TIMESTAMP="@${SOURCE_DATE_EPOCH}"
export KBUILD_BUILD_USER=builduser
export KBUILD_BUILD_HOST=buildhost

export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
cp mnt-reform-defconfig .config
make -j $(nproc)
