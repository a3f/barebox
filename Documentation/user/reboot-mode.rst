.. _reboot_mode:

Reboot Mode
-----------

This driver parses the reboot commands like "reboot bootloader"
and "reboot recovery" to get a boot mode described in the
device tree , then call the write interface to store the boot
mode in some place like special register or SRAM, which can
be read by the bootloader after system reboot. The bootloader
can then take different action according to the mode stored.

This is commonly used on Android based devices in order to
reboot the device into fastboot or recovery mode.

To handle a device in a secure and safe manner many applications are using
a watchdog or other ways to reset a system to bring it back into life if it
hangs or crashes somehow.

In these cases the hardware restarts and runs the bootloader again. Depending on
the root cause of the hang or crash, the bootloader sometimes should not just
re-start the main system again. Maybe it should do some kind of recovery instead.
For example it should wait for another update (for the case the cause of a
crash is a failed update) or should start into a fall back system instead.

In order to handle failing systems gracefully the bootloader needs the
information why it runs. This is called the "reset reason". It is provided by
the global variable ``system.reboot_mode`` and can be used in scripts via
``$global.system.reboot_mode``.

Difference to barebox state
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Difference to barebox booutsource
~~~~~~~~~~~~~~~~~~~~~~~~~~~
