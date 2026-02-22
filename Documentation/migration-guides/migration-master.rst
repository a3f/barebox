:orphan:

Clock mux rate selection
------------------------

The default rate selection for mux clocks has been aligned with Linux.
Previously, barebox mux clocks selected the parent whose rate was closest
to the requested rate. Now, the default behavior is to select the highest
parent rate that does not exceed the requested rate (round-down).

Drivers that need the old closest-rate behavior should set the
``CLK_MUX_ROUND_CLOSEST`` flag on the mux clock.

ARM NXP i.MX8MP
---------------

On NXP i.MX8MP the SoC UID was read out wrong. It really is 128bit from which
barebox only read 64bit. barebox now does it correctly, but rolled out devices
might depend on the SoC UID being constant. In that case
CONFIG_ARCH_IMX8MP_KEEP_COMPATIBLE_SOC_UID should be enabled.
