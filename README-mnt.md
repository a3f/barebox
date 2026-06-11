# Barebox for RK3588 based MNT Reform Series

## Build Notes

```
git clone https://github.com/rockchip-linux/rkbin

export ARCH=arm64
# export CROSS_COMPILE=aarch64-linux-gnu-
cp mnt-reform-defconfig .config

cp rkbin/bin/rk35/rk3588_ddr_lp4_2112MHz_lp5_2400MHz_v1.19.bin arch/arm/boards/mnt-reform2-rk3588/sdram-init.bin

# UPDATE: probably _don't_ do this
#   cp rkbin/bin/rk35/rk3588_bl31_v1.51.elf firmware/rk3588-bl31.bin
# but get upstream TF-A instead, see https://source.mnt.re/reform/reform-rk3588-uboot/-/blob/main/build.sh?ref_type=heads#L30
# a binary for your convenience included here now: firmware/rk3588-bl31.bin

make menuconfig
make -j8
```

## Install

Create a bootable microSD card by copying the barebox image for your hardware:

`sudo dd if=images/barebox-mnt-reform2-rk3588.img of=/dev/sdX seek=64`

(Note the seek to make sure that the partitioning doesn't conflict with the area barebox is written to).

## Note: Erase eMMC u-boot first stage to be able to test microSD boot

`sudo dd if=/dev/zero bs=512 seek=64 of=/dev/mmcblk0 count=2`

## Flashing to eMMC

**WARNING:**
It is highly recommended to test the barebox image by booting it off microSD first before flashing to emmc. If there is an issue with the barebox image, or a bad flash, the system may no longer be able to boot and will likely require specific hardware to recover.

### From within linux:

After testing the barebox microSD image, the same image can be written to emmc by copying the microSD card contents to emmc:

```bash
# Recommended: Create a backup first
sudo dd if=/dev/mmcblk0 bs=1M count=10 of=emmc-backup.bin
# Copy the contents of the microSD card to emmc
sudo dd if=/dev/sdX of=/dev/mmcblk0 bs=1M count=10
```

or by writing the barebox image to emmc directly similarly to how the bootable microSD card was created:

`sudo dd if=/path/to/barebox.img of=/dev/mmcblk0 seek=64`

### From within barebox

Barebox can flash a barebox image to emmc and if it's able to will do so in a fail safe way. To do this, put the barebox image on an external USB storage device, plug it into the reform and then boot barebox. You may need to run `usb` for the external drive to be detected if barebox was booted first. The device file should appear as /dev/diskX, and should automount when /mnt/diskX.Y is accessed, but you may need mount it manually if this doesn't happen. Once the external drive with the barebox image is mounted and accessible within barebox, run the following command to flash barebox to emmc:

`barebox_update -d /dev/mmc0 /path/to/barebox.img`

Note: Sometimes this command can fail the first time with an IO error. If this happens, try re-running the command. 

The same process can be used to update barebox.

## Uninstall

To revert back to u-boot, boot the Reform system image from microSD and run 

`reform-flash-bootloader emmc`

If this fails with a message that existing partitions would be overidden by the bootloader, you may need to delete or move partitions.

If for some reason barebox cannot boot the system image from microSD, you can use barebox's erase command to wipe the emmc so that microSD will be booted from instead:

`erase /dev/mmc0 0x8000+0x10M`

## Booting OpenBSD

Note: OpenBSD is configured to use the framebuffer console by default. To use the serial console instead, run the following from Barebox before booting OpenBSD:
```
fb0.register_simplefb=enabled
```

There are a couple of device drivers that are known to cause OpenBSD to hang when booting. These drivers can be disabled by entering `boot -c` at OpenBSD's `boot>` prompt, then enter the following at the `UKC>` prompt:
```
UKC> disable rkusbdpphy*
UKC> disable rkdrm*
UKC> quit
```

Keyboard input may not be functional at this stage however. In this case, you can disable these devices from Barebox using [of_property](https://www.barebox.org/doc/latest/commands/misc/of_property.html#command-of-property) before booting OpenBSD:

```
of_property -s -f /phy@fed80000/ status disabled
of_property -s -f /phy@fed90000/ status disabled
of_property -s -f /display-subsystem/ status disabled
boot
```

It is recommended to create a new kernel configuration disabling these drivers using the [config](https://man.openbsd.org/config.8) tool to avoid needing to do this on every boot.  Once booted into OpenBSD create `/etc/bsd.re-config` with the following contents:
```
disable rkusbdpphy*
disable rkdrm*
```
Then run:
```
# Back up the existing kernel
cp /bsd /bsd.bak

# Overwrite the kernel with the new config
config -e -c /etc/bsd.re-config -f /bsd
```

See also: https://man.openbsd.org/boot_config.8


