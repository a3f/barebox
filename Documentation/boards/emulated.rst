Emulated targets
================

Some targets barebox runs on are virtualized by emulators like QEMU, which
allows basic testing of barebox functionality without the actual hardware.

Generic DT image
----------------

Supported for ARM and RISC-V. It generates a barebox image that can
be booted with the Linux kernel booting convention, which makes
it suitable to be passed as argument to the QEMU ``-kernel`` option.
The device tree is either the QEMU internal device tree or a device
tree supplied by QEMU's ``-dtb`` option. The former can be very useful
with :ref:`virtio_sect`, because QEMU will fix up the virtio mmio
regions into the device tree and barebox will discover the devices
automatically, analogously to what it does with VirtIO over PCI.

test/emulate.pl
---------------

The ``emulate.pl`` script shipped with barebox can be used to easily
start VMs. It reads a number of YAML files in ``test/``, which describe
some virtualized targets that barebox is known to run on.
Controlled by command line options, these targets are built with
tuxmake if available and loaded into the emulator for either interactive
use or for automated testing with Labgrid ``QEMUDriver``.

.. _tuxmake: https://pypi.org/project/tuxmake/
.. _Labgrid: https://labgrid.org

Install dependencies for interactive use::

  cpan YAML::Tiny # or use e.g. libyaml-tiny-perl on debian
  pip3 install tuxmake # optional

Example usage::

  # Switch to barebox source directory
  cd barebox

  # emulate ARM virt with an image built by vexpress_defconfig
  ARCH=arm ./test/emulate.pl virt@vexpress_defconfig

  # build all MIPS targets known to emulate.pl and exit
  ARCH=mips ./test/emulate.pl --no-emulate

The script can also be used from a finished barebox build directory::

  # Switch to build directory
  cd build

  # run a barebox image built outside tuxmake with an emulated vexpress-ca15
  ARCH=x86 ./test/emulate.pl efi_defconfig --no-tuxmake

  # run tests instead of starting emulator interactively
  ARCH=x86 ./test/emulate.pl efi_defconfig --no-tuxmake --test

``emulate.pl`` also has some knowledge on paravirtualized devices::

  # Run target and pass a block device (here /dev/virtioblk0)
  ARCH=riscv ./test/emulate.pl --blk=rootfs.ext4 virt64_defconfig

``emulate.pl`` if needed command line options can be passed directly
to the emulator/``labgrid-pytest`` as well by placing them behind ``--``::

  # appends -device ? to the command line. Add -n to see the final result
  ARCH=riscv ./test/emulate.pl virt64_defconfig -- -device ?

For a complete listing of options run ``./test/emulate.pl -h``.
