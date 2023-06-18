Emulated targets
================

Some targets barebox runs on are virtualized by emulators like QEMU, which
allows basic testing of barebox functionality without the actual hardware.

Generic DT image
----------------

Supported for ARM and RISC-V. It generates a barebox image that can
be booted with the Linux kernel booting convention, which makes
it suitable to be passed as argument to the QEMU ``-kernel`` option
(as well as booted just like Linux from barebox or other bootloaders).

The device tree can be passed externally via QEMU's ``-dtb`` option, but
also the QEMU internal device tree can be used.

The latter can be very useful with :ref:`virtio_sect`, because QEMU will
fix up the virtio mmio regions into the device tree and barebox will
discover the devices automatically, analogously to what it does with
VirtIO over PCI.

labgrid-pytest
--------------

barebox ships with a number of Labgrid environment YAML files in
``test/$ARCH``, which describe some virtualized targets that barebox
is known to run on. See the labgrid docs for how to install it or
use the barebox container by prefixing ``pytest`` invocations
with ``./scripts/container.sh``.

.. _Labgrid: https://labgrid.org

Example usage::

  # Switch to barebox source directory
  cd barebox

  export LG_BUILDDIR=build-x86

  # emulate x86 VM runnig the EFI payload from efi_defconfig
  scripts/container.sh ./MAKEALL -a x86 -O $LG_BUILDDIR efi_defconfig

  scripts/container.sh pytest --lg-env test/x86/efi_defconfig.yaml

The script can also be used with a precompiled barebox tree::

  export LG_BUILDDIR=/path/to/barebox/build

  # run barebox interactively
  pytest --lg-env test/arm/virt@multi_v7_defconfig.yaml --interactive

The ``pytest`` setup also has some knowledge of paravirtualized devices::

  # Run target and pass a block device (here /dev/virtioblk0)
  pytest --lg-env=test/arm/multi_v8_defconfig.yaml --blk=rootfs.ext4 --interactive

Extra command line options for Qemu can be passed directly by prefixing
each option with ``--qemu``::

  # appends -device ? to the command line. Add -n to see the final result
  pytest --lg-env=test/riscv/rv64i_defconfig.yaml --interactive --qemu=-device --qemu=?

For more flexibility, the used Qemu command line can be dumped by passing
``--dry-run`` and then used independently::

  # appends -device ? to the command line. Add -n to see the final result
  pytest --lg-env=test/riscv/rv64i_defconfig.yaml --interactive --dry-run

For a complete listing of options run ``pytest -h``.
