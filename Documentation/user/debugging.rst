Debugging with GDB
==================

Barebox can be configured to break on prebootloader and main barebox entry. This
breakpoint can not be resumed and will stop the board to allow the user to
attach a JTAG debugger with OpenOCD. Additionally, barebox provides helper
scripts to load the symbols from the ELF binaries.
The python scripts require `pyelftools`.

Setup
-----

- Create a virtual barebox machine with QEMU or debug your system using OpenOCD.
  barebox already ships with a number of virtual machine definitions in the form
  of Labgrid environment config files that you can use.

- Build barebox with ``CONFIG_GDB_SCRIPTS`` enabled. If your architecture supports
  ``CONFIG_FRAME_POINTER``, keep it enabled. TODO: note about CONFIG_PBL_BREAK.

- Build the gdb scripts::

    make scripts_gdb

- Run the GDB server::

    - for "standard" targets like ``multi_v8_defconfig`` via Labgrid by
      simply running ``pytest --interactive --gdb``.

  or

    - at QEMU VM startup time by appending "-s" to the QEMU command line
      manually

  or

    - during QEMU runtime by issuing "gdbserver" from the QEMU monitor
      console

  or

    - by using the gdb server implementation in OpenOCD

- cd /path/to/barebox-build

- Start gdb: gdb barebox

  This will automatically load the ``barebox-gdb.py`` script installed before

  Note: Some distros may restrict auto-loading of gdb scripts to known safe
  directories. In case gdb reports to refuse loading barebox-gdb.py, add::

    add-auto-load-safe-path /path/to/barebox-build

  to ~/.gdbinit. See gdb help for more details.

- Attach to the booted guest::

    - Default for QEMU: ``(gdb) target remote :1234``

  or

    - Default for OpenOCD: ``(gdb) target remote :3333``

Usage
-----

Two new commands will be available in gdb, `bb-load-symbols` and
`bb-skip-break`. `bb-load-symbols` can load either the main `barebox` file or
one of the .pbl files in the image directories. The board needs to be stopped in
either the prebootloader or main barebox breakpoint, and gdb needs to be
connected to OpenOCD. To continue booting the board, `bb-skip-break` jumps over
the breakpoint and continues the barebox execution.
