.. _bareboxdt:

Barebox devicetree handling and bindings
========================================

The preferred way of adding board support to barebox is to have devices
on non-enumerable buses probed from device tree.
barebox provide both the Linux OpenFirmware ``of_*`` and the libfdt ``fdt_`` APIs
for device tree parsing. The former makes porting the device tree specific
bits from Linux device drivers very straight forward, while the latter can be
used for very early (PBL) handling of flattened device trees, should this be
necessary.

Additionally, barebox has support for programmatically fixing up device trees
it passes to the kernel, either directly via ``of_register_fixup`` or via device
tree overlays.

Upstream Device Trees
---------------------

barebox regularly synchronizes with the Linux kernel device tree definitions
via the `kernel.org Split device-tree repository`_.
They are located under the top-level ``dts/`` directory.

Patches against ``dts/`` and its subdirectories are not accepted upstream.

.. _kernel.org Split device-tree repository: https://git.kernel.org/pub/scm/linux/kernel/git/devicetree/devicetree-rebasing.git/

barebox Device Trees
--------------------

For supporting architectures, barebox device trees are located in
``arch/$ARCH/dts``. Usually the barebox ``board.dts`` imports the upstream
device tree under ``dts/src/$ARCH`` with ``#include "$ARCH/board.dts"`` and
then extends it with barebox-specifics like :ref:`barebox,state`,
environment or boot-time device configuration.

Device Tree probing largely happens via compatible properties with no special
meaning to the node names themselves. It's thus paramount that any device tree
nodes extended in the barebox device tree are referenced by label (e.g.
``<&phandle>``, not by path, to avoid run-time breakage like this::

  # Upstream dts/src/$ARCH/board.dts
  / {
  	leds {
            led-red { /* formerly named red when the barebox DTS was written */
            	/* ... */
            };
        };
  };

  # barebox arch/$ARCH/dts/board.dts
  #include <$ARCH/board.dts>
  / {
  	leds {
            red {
                barebox,default-trigger = "heartbeat";
            };
        };
  };

In the previous example, a device tree sync with upstream resulted in a regression
as the former override became a new node with a single property without effect.

The preferred way around this is to use labels directly::

  # Upstream dts/src/$ARCH/board.dts
  / {
  	leds {
            status_led: red { };
        };
  };

  # barebox arch/$ARCH/dts/board.dts
  #include <$ARCH/board.dts>

  &status_led {
      barebox,default-trigger = "heartbeat";
  };

If there's no label defined upstream for the node, but for a parent,
a new label can be constructed from that label and a relative path::

  # Upstream dts/src/$ARCH/board.dts
  / {
  	led_controller: leds {
            red { };
        };
  };

  # barebox arch/$ARCH/dts/board.dts
  #include <$ARCH/board.dts>

  &{led_controller/red} {
      barebox,default-trigger = "heartbeat";
  };

As last resort, the full path shall be used::

  &{/leds/red} {
      barebox,default-trigger = "heartbeat";
  };

Any of these three approaches would lead to a compile error should the
``/leds/red`` path be renamed or removed. This also applies to uses
of ``/delete-node/``.

Only exception to this rule are well-known node names that are specified by
the `specification`_ to be parsed by name. These are: ``chosen``, ``aliases``
and ``cpus``, but **not** ``memory``.

.. _specification: https://www.devicetree.org/specifications/

.. _external_dts_fragments:

External Device Tree Fragments
------------------------------

Device tree changes that are specific to a product, but shouldn't be carried
as patches against the barebox tree, can be supplied from outside as dts
fragments. ``CONFIG_EXTERNAL_DTS_FRAGMENTS`` takes a space-separated list of
dts files that are appended, in order, to the source of every device tree
barebox builds::

  CONFIG_EXTERNAL_DTS_FRAGMENTS="/path/to/first.dtsi /path/to/second.dtsi"

As the same fragments are used for all device trees of a multi-image build,
a preprocessor macro derived from the name of the main dts file is defined
while it's compiled, e.g. ``foo_board_dts`` for ``foo-board.dts``, so
fragment content can be limited to a single image::

  #ifdef foo_board_dts
  / {
          /* only applied to foo-board.dtb */
  };
  #endif

Once ``CONFIG_EXTERNAL_DTS_FRAGMENTS`` is set, ``CONFIG_EXTERNAL_DTS_ONLY``
becomes selectable. It discards the contents of the board device tree
sources, so the fragments alone make up the resulting device trees, which
allows replacing the device tree of all enabled boards without patching
barebox. The fragments are applied to the empty fallback device tree from
``common/fallback.dts`` in that case, so image-specific content is guarded
with ``#ifdef fallback_dts`` instead. Device tree overlays (``.dtbo``) keep
their source as-is either way.

Both options are meant to be set by an external build system, like Yocto or
buildroot, not to be put into barebox' defconfig files. Because
``CONFIG_EXTERNAL_DTS_ONLY`` is hidden until the fragments are configured,
it can't be enabled on its own, which would leave barebox with empty device
trees.

Device Tree Compiler
--------------------

barebox makes use of the ``dtc`` and ``fdtget`` and the underlying ``libfdt``
from the `Device-Tree Compiler`_ project.

.. _Device-Tree Compiler: https://git.kernel.org/pub/scm/utils/dtc/dtc.git

These utilities are built as part of the barebox build process. Additionally,
libfdt is compiled once more as part of the ``CONFIG_BOARD_GENERIC_DT``
if selected.

Steps to update ``scripts/dtc``:

* Place a ``git-checkout`` of the upstream ``dtc`` directory in the parent
  directory of your barebox ``git-checkout``.
* Run ``scripts/dtc/update-dtc-source.sh`` from the top-level barebox directory.
* Wait till ``dtc`` build, test, install and commit conclude.
* Compile-test with ``CONFIG_BOARD_GENERIC_DT=y``.
* If ``scripts/dtc/Makefile`` or barebox include file changes are necessary,
  apply them manually in a commit preceding the ``dtc`` update.

barebox-specific Bindings
-------------------------

Contents:

.. toctree::
   :glob:
   :maxdepth: 1

   bindings/barebox/*
   bindings/clocks/*
   bindings/firmware/*
   bindings/leds/*
   bindings/misc/*
   bindings/mtd/*
   bindings/power/*
   bindings/regulator/*
   bindings/rtc/*
   bindings/watchdog/*

Automatic Boot Argument Fixups to the Devicetree
------------------------------------------------

barebox automatically fixes up some boot and system information in the device tree.

In the device tree root, barebox fixes up

 * serial-number (if available)
 * machine compatible (if overridden)

In the ``chosen``-node, barebox fixes up

 * barebox-version
 * reset-source
 * reset-source-instance (if available)
 * reset-source-device (node-path, only if available)
 * bootsource
 * boot-hartid (only on RISC-V)

These values can be read from the booted linux system in ``/proc/device-tree/``
or ``/sys/firmware/devicetree/base``.

.. _of_diff:

To see a dry run of what barebox would fixup, the ``of_diff`` command can be
used::

  # Diff before and after applying fixups on barebox DT
  of_diff - +

  # Diff kernel device tree before and after fixups
  of_diff /mnt/mmc2.0/boot/imx6q-tx6q.dtb +
