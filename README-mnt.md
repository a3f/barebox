# Barebox for RK3588 based MNT Reform Series

## Build Notes

```
git clone https://github.com/rockchip-linux/rkbin

export ARCH=arm64
# export CROSS_COMPILE=aarch64-linux-gnu-
cp mnt-reform-defconfig .config

cp rkbin/bin/rk35/rk3588_ddr_lp4_2112MHz_lp5_2400MHz_v1.19.bin arch/arm/boards/mnt-reform2-rk3588/sdram-init.bin
cp rkbin/bin/rk35/rk3588_bl31_v1.51.elf firmware/rk3588-bl31.bin

make menuconfig
make -j8
```

## Install

sudo dd if=images/barebox-mnt-reform2-rk3588.img of=/dev/sdX seek=64

## Note: Erase eMMC u-boot first stage to be able to test microSD boot

dd if=/dev/zero bs=512 seek=64 of=/dev/mmcblk0 count=2

