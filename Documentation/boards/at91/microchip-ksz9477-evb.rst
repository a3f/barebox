Microchip KSZ 9477 Evaluation board
===================================

This is an evaluation board for the KSZ9477 switch that uses the sama5d36 CPU.
The board uses Device Tree and supports multi image.

Building barebox as second stage bootloader:

.. code-block:: sh

  make ARCH=arm microchip_ksz9477_evb_defconfig

There are also a separate defconfig for operating barebox as first stage
bootloader originating from SD Card.
This configuration doesn't use the device-tree as the NVM bootloader
(SoC ROM code) requires the first stage bootloader to fit into 64K.

Generally, the first stage may comes from any of the following boot
sources (in that order):

* SPI0 CS0 Flash
* SD Card
* NAND Flash
* SPI0 CS1 Flash
* I2C EEPROM

After being loaded into SRAM by the NVM bootloader, the first stage does low
level clock initialization, configuration of the DDRAM controller and
bootstraps the second stage boot loader.

SD Card Bootstrap
-----------------

For boot from SD card, barebox additionally needs to be configured as
first stage bootloader:

.. code-block:: sh

  make ARCH=arm microchip_ksz9477_evb_first_stage_mmc_defconfig

The resulting barebox image must be renamed to ``BOOT.BIN``
and located in the root directory of the first FAT16/32 partition
on the SD Card/eMMC. After initialization, ``BOOT.BIN`` will look
for a ``barebox.bin`` in the same directory and load and execute it
from DRAM.
