:orphan:

global.partitions.first_usable_lba removed
------------------------------------------

The ``global.partitions.first_usable_lba`` variable has been removed.
Use ``global.partitions.first_partition_offset`` instead.

The new variable is a byte offset used by free-space searches for new
partitions, for example ``parted mkpart_size``. The default is
``8388608`` bytes (8 MiB). To keep an old configuration, multiply the
old ``first_usable_lba`` value by 512.

ARCH=arm64
----------

Use of ``ARCH=arm`` for 64-bit ARM builds is deprecated and now emits
a warning. Users should change build scripts to use ``ARCH=arm64``
instead when targetting ARMv8.

Removal of deprecated CONFIG_BOOTM_OPTEE
----------------------------------------

The support for late loading of OP-TEE had been deprecated and ultimately
removed as it greatly increased the attack surface and was only supported
on 32-bit ARM systems.

OP-TEE loading is now only supported
:ref:`in the prebootloader <optee_early_loading>`.

For i.MX6 boards, this can be enabled by enabling
``CONFIG_FIRMWARE_IMX6_OPTEE``.

Using bootm with EFI applications
---------------------------------

In older versions, barebox employed heuristics to decide whether an EFI
application is a Linux kernel or not. The result determined whether
bootargs are serialized into kernel command-line options and, for
barebox itself running as EFI application (payload), also whether return
to barebox is permitted.

In newer versions, barebox will treat every UEFI application booted via
:ref:`command_bootm` as kernel. To boot UEFI applications of type
``MS-DOS executable`` without the kernel logic, they should instead be
executed directly::

  barebox@barebox EFI payload:/ /boot/myapp.efi
