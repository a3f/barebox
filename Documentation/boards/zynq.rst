Xilinx Zynq 7000
================

Barebox has support for the Xilinx Zynq 7000.

Image creation
--------------

The Zynq defconfig supports the Avnet ZedBoard. Use it to build the Barebox image::

   make ARCH=arm zynq_defconfig
   make ARCH=arm

Create a FAT partition as the first partition of the SD card and copy the
produced image ``images/barebox-avnet-zedboard.img`` into this partition.
Rename the image to ``BOOT.bin`` which is the name the Primary Bootloader of the
Zynq 7000 expects for the next stage.

Booting second stage from the FSBL
----------------------------------

Instead of replacing the vendor First Stage Boot Loader (FSBL), barebox can
also be started as a second stage after it. bootgen, which generates the
``BOOT.bin`` loaded by the BootROM, takes the load and execution address of
second stage partitions from the ELF program headers. Build the generic
second stage image as an ELF executable for this purpose:

* Enable ``CONFIG_BOARD_GENERIC_DT_ELF`` and set
  ``CONFIG_BOARD_GENERIC_DT_ELF_LOADADDR`` to an address in DDR, e.g.
  ``0x04000000``.
* The FSBL passes no device tree, so the image boots the built-in device
  tree assembled from the external dts fragments: set
  ``CONFIG_EXTERNAL_DTS_FRAGMENTS`` to a full board description, including
  the ``/memory`` node, and enable ``CONFIG_EXTERNAL_DTS_ONLY``, which
  becomes selectable once the fragments are configured. See
  :ref:`second_stage` and :ref:`external_dts_fragments` for details.

Then let bootgen combine the FSBL and ``images/barebox-dt-2nd.elf`` into a
``BOOT.bin``, using a BIF file like::

   the_ROM_image:
   {
       [bootloader] fsbl.elf
       barebox-dt-2nd.elf
   }

.. code-block:: console

   bootgen -arch zynq -image boot.bif -o BOOT.bin -w on

Bitstream loading
-----------------

The Zynq 7000 features an ARM Cortex-A9 processor (Processing System, PS)
alongside a Programmable Logic (PL) component that functions as an FPGA. Barebox
provides support for loading a bitstream into the PL through its firmware
interface.
