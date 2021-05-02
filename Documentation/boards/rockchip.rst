Rockchip RK3188
===============

Radxa Rock
----------

Radxa Rock is a small SBC based on Rockchip RK3188 SoC.
See http://radxa.com/Rock for additional information.

Building
^^^^^^^^

.. code-block:: sh

  make ARCH=arm radxa_rock_defconfig
  make ARCH=arm

Creating bootable SD card
^^^^^^^^^^^^^^^^^^^^^^^^^

This will require a DRAM setup blob and additional utilities, see above.

Card layout (block == 0x200 bytes).

============   ==========================================
Block Number   Name
============   ==========================================
0x0000         DOS partition table
0x0040         RK bootinfo (BootROM check sector)
0x0044         DRAM setup routine
0x005C         Bootloader (barebox)
0x0400         Barebox environment
0x0800         Free space start
============   ==========================================

Instructions.

* Make 2 partitions on SD for boot and root filesystems.
* Checkout and compile https://github.com/apxii/rkboottools
* Get some RK3188 bootloader from https://github.com/neo-technologies/rockchip-bootloader
* Run "rk-splitboot RK3188Loader(L)_V2.13.bin" command. (for example).
  You will get FlashData file with others. It's a DRAM setup blob.
* Otherwise it can be borrowed from RK U-boot sources from
  https://github.com/linux-rockchip/u-boot-rockchip/blob/u-boot-rk3188/tools/rk_tools/3188_LPDDR2_300MHz_DDR3_300MHz_20130830.bin
* Run "rk-makebootable FlashData barebox-radxa-rock.bin rrboot.bin"
* Insert SD card and run "dd if=rrboot.bin of=</dev/sdcard> bs=$((0x200)) seek=$((0x40))"
* SD card is ready

Rockchip RK356x
===============

Barebox features support for the Rockchip RK3566 and RK3568 SoCs, where the
RK3566 can be considered as reduced but largely identical version of the
RK3568.

Supported Boards
----------------

- Rockchip RK3568 EVB
- Rockchip RK3568 Bananapi R2 Pro
- Pine64 Quartz64 Model A
- Radxa ROCK3 Model A
- Radxa CM3 (RK3566) IO Board

The steps described in the following target the RK3568 and the RK3568 EVB but
generally apply to both SoCs and all boards.
Differences between the SoCs or boards are outlined where required.

Building
--------

The build process needs three binary files which have to be copied from the
`rkbin https://github.com/rockchip-linux/rkbin` repository to the barebox source tree:

.. code-block:: sh

  cp $RKBIN/bin/rk35/rk3568_bl31_v1.34.elf firmware/rk3568-bl31.bin
  cp $RKBIN/bin/rk35/rk3568_bl32_v2.08.bin firmware/rk3568-op-tee.bin
  cp $RKBIN/bin/rk35/rk3568_ddr_1560MHz_v1.13.bin arch/arm/boards/rockchip-rk3568-evb/sdram-init.bin

With these barebox can be compiled as:

.. code-block:: sh

  make ARCH=arm rockchip_v8_defconfig
  make ARCH=arm

**NOTE** I found the bl32 firmware non working for me as of 7d631e0d5b2d373b54d4533580d08fb9bd2eaad4 in the rkbin repository.

**NOTE** The RK3566 and RK3568 seem to share the bl31 and bl32 firmware files,
whereas the memory initialization blob is different.

Creating a bootable SD card
---------------------------

A bootable SD card can be created with:

.. code-block:: sh

  dd if=images/barebox-rk3568-evb.img of=/dev/sdx bs=1024 seek=32

The barebox image is written to the raw device, so make sure the partitioning
doesn't conflict with the are barebox is written to. Starting the first
partition at offset 8MiB is a safe bet.

USB bootstrapping
-----------------

The RK3568 can be bootstrapped via USB for which the rk-usb-loader tool in the barebox
repository can be used. The tool takes the same images as written on SD cards:

.. code-block:: sh

  ./scripts/rk-usb-loader images/barebox-rk3568-evb.img

Note that the boot order of the RK3568 is not configurable. The SoC will only enter USB
MaskROM mode when no other bootsource contains a valid bootloader. This means to use USB
you have to make all other bootsources invalid by removing SD cards and shortcircuiting
eMMCs. The RK3568 EVB has a pushbutton to disable the eMMC.

On the Quartz64 boards, remove the eMMC module if present.

Rockchip RK339
==============

Pine64 ROCKPRO64
-----------------

Building
^^^^^^^^

The build process needs the DDR initialization blob, which can be copied from the
`rkbin https://github.com/rockchip-linux/rkbin` repository to the barebox source tree:

.. code-block:: sh
  cp $RKBIN/bin/rk33/rk3399_ddr_800MHz_v1.25.bin arch/arm/boards/pine64-rockpro64/sdram-init.bin

You'll also need ARM Trusted Firmware running in EL3:

.. code-block:: sh

  make CROSS_COMPILE=aarch64-linux-gnu- PLAT=rk3399 bl31
  cp rk3399/release/bl31/bl31.elf ../barebox/firmware/rk3399-bl31.bin

With these barebox can be compiled as:

.. code-block:: sh

  make ARCH=arm rockchip_v8_defconfig
  make ARCH=arm


Radxa ROCK Pi N10
-----------------

Building
^^^^^^^^

The build process needs the DDR initialization blob, which can be copied from the
`rkbin https://github.com/rockchip-linux/rkbin` repository to the barebox source tree:

.. code-block:: sh
  cp $RKBIN/bin/rk33/rk3399pro_ddr_800MHz_v1.25.bin arch/arm/boards/radxa-rock-pi-n10/sdram-init.bin

You'll also need ARM Trusted Firmware running in EL3:

.. code-block:: sh

  make CROSS_COMPILE=aarch64-linux-gnu- PLAT=rk3399 bl31
  cp rk3399/release/bl31/bl31.elf ../barebox/firmware/rk3399-bl31.bin

**NOTE** At least ``rk3399pro_bl31_v1.35`` from the rkbin repository hangs
before returning to barebox. Use upstream TF-A as BL31 instead.

With these barebox can be compiled as:

.. code-block:: sh

  make ARCH=arm rockchip_v8_defconfig
  make ARCH=arm

Creating a bootable SD card
^^^^^^^^^^^^^^^^^^^^^^^^^^^

A bootable SD card can be created with:

.. code-block:: sh

  dd if=images/barebox-radxa-rock-pi-n10.img of=/dev/sdx bs=1024 seek=32

The barebox image is written to the raw device, so make sure the partitioning
doesn't conflict with the are barebox is written to. Starting the first
partition at offset 8MiB is a safe bet.

USB bootstrapping
^^^^^^^^^^^^^^^^^

The RK3399 can't (yet) be bootstrapped via USB with the rk-usb-loader tool
in the barebox repository. rkdevelop can be used to write it to eMMC though:

.. code-block:: sh

    # use the appropriate loader from $RKBIN/bin/rk33/
    rkdeveloptool db ~/Downloads/rk3399pro_loader_v1.20.115.bin
    # write to sector 64
    rkdeveloptool wl 64 barebox-radxa-rock-pi-n10.img
    # reset
    rkdeveloptool rd

To enter MaskROM mode, remove SD and push the ``RECOVER`` button during reset.

Rockchip PX30
=============

iesy RPX30 OSM
--------------

Unlike RK3399 and RK3568, where the barebox image is directly loaded by the
BootROM, PX30 support in barebox is not that advanced yet and barebox requires
an idbloader to boot it.

Building
^^^^^^^^

The build process needs the DDR initialization blob, which can be copied from the
`rkbin https://github.com/rockchip-linux/rkbin` repository to the barebox source tree:

.. code-block:: sh
  cp $RKBIN/bin/rk33/rk3399pro_ddr_800MHz_v1.25.bin arch/arm/boards/radxa-rock-pi-n10/sdram-init.bin

You'll also need ARM Trusted Firmware running in EL3:

.. code-block:: sh

  make CROSS_COMPILE=aarch64-linux-gnu- PLAT=rk3399 bl31
  cp rk3399/release/bl31/bl31.elf ../barebox/firmware/rk3399-bl31.bin

**NOTE** At least ``rk3399pro_bl31_v1.35`` from the rkbin repository hangs
before returning to barebox. Use upstream TF-A as BL31 instead.

With these barebox can be compiled as:

.. code-block:: sh

  make ARCH=arm rockchip_v8_defconfig
  make ARCH=arm

Creating a bootable SD card
^^^^^^^^^^^^^^^^^^^^^^^^^^^

A bootable SD card can be created with:

.. code-block:: sh

  ../rkbin/tools/loaderimage --pack --uboot images/start_px30_iesy_osm.pblb barebox.bin 0x200000
  dd if=barebox.bin of=/dev/mmcblk0 bs=1024 seek=8192

The barebox image is written to the raw device, so make sure the partitioning
doesn't conflict with the are barebox is written to. Starting the first
partition at offset 8MiB is a safe bet.

USB bootstrapping
^^^^^^^^^^^^^^^^^

The RK3399 can't (yet) be bootstrapped via USB with the rk-usb-loader tool
in the barebox repository. rkdevelop can be used to write it to eMMC though:

.. code-block:: sh

    # use the appropriate loader from $RKBIN/bin/rk33/
    rkdeveloptool db ~/Downloads/rk3399pro_loader_v1.20.115.bin
    # write to sector 64
    rkdeveloptool wl 64 barebox-radxa-rock-pi-n10.img
    # reset
    rkdeveloptool rd

To enter MaskROM mode, remove SD and push the ``RECOVER`` button during reset.
