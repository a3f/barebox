.. _changes:

Minimal requirements to compile the Kernel
++++++++++++++++++++++++++++++++++++++++++

Intro
=====

This document is designed to provide a list of the minimum levels of
software necessary to run the current kernel version.

This document is originally based on my "Changes" file for 2.0.x kernels
and therefore owes credit to the same people as that file (Jared Mauch,
Axel Boldt, Alessandro Sigala, and countless other users all over the
'net).

Current Minimal Requirements
****************************

Upgrade to at **least** these software revisions before thinking you've
encountered a bug!  If you're unsure what version you're currently
running, the suggested command should tell you.

Again, keep in mind that this list assumes you are already functionally
running a Linux kernel.  Also, not all tools are necessary on all
systems; obviously, if you are not signing an i.MX bootloader, you
don't need the i.MX code signing tool.

====================== ===============  ========================================
        Program        Minimal version       Command to check the version
====================== ===============  ========================================
GNU C                  8.1              gcc --version
Clang/LLVM (optional)  13.0.1           clang --version
Rust (optional)        1.78.0           rustc --version
bindgen (optional)     0.65.1           bindgen --version
GNU make               4.0              make --version
bash                   4.2              bash --version
binutils               2.30             ld -v
flex                   2.5.35           flex --version
bison                  2.0              bison --version
pahole                 1.16             pahole --version
util-linux             2.10o            mount --version
kmod                   13               depmod -V
e2fsprogs              1.41.4           e2fsck -V
jfsutils               1.1.3            fsck.jfs -V
xfsprogs               2.6.0            xfs_db -V
squashfs-tools         4.0              mksquashfs -version
btrfs-progs            0.18             btrfs --version
pcmciautils            004              pccardctl -V
quota-tools            3.09             quota -V
PPP                    2.4.0            pppd --version
nfs-utils              1.0.5            showmount --version
procps                 3.2.0            ps --version
udev                   081              udevd --version
grub                   0.93             grub --version || grub-install --version
mcelog                 0.6              mcelog --version
iptables               1.4.2            iptables -V
openssl & libcrypto    1.0.0            openssl version
bc                     1.06.95          bc --version
Sphinx\ [#f1]_         3.4.3            sphinx-build --version
GNU tar                1.28             tar --version
gtags (optional)       6.6.5            gtags --version
mkimage (optional)     2017.01          mkimage --version
Python (optional)      3.9.x            python3 --version
GNU AWK (optional)     5.1.0            gawk --version
====================== ===============  ========================================

.. [#f1] Sphinx is needed only to build the Kernel documentation

Kernel compilation
******************

GCC
---

The gcc version requirements may vary depending on the type of CPU in your
computer.

Clang/LLVM (optional)
---------------------

The latest formal release of clang and LLVM utils (according to
`releases.llvm.org <https://releases.llvm.org>`_) are supported for building
kernels. Older releases aren't guaranteed to work, and we may drop workarounds
from the kernel that were used to support older versions. Please see additional
docs on :ref:`Building Linux with Clang/LLVM <kbuild_llvm>`.

Rust (optional)
---------------

A recent version of the Rust compiler is required.

Please see Documentation/rust/quick-start.rst for instructions on how to
satisfy the build requirements of Rust support. In particular, the ``Makefile``
target ``rustavailable`` is useful to check why the Rust toolchain may not
be detected.

bindgen (optional)
------------------

``bindgen`` is used to generate the Rust bindings to the C side of the kernel.
It depends on ``libclang``.

Make
----

You will need GNU make 4.0 or later to build the kernel.

Bash
----

Some bash scripts are used for the kernel build.
Bash 4.2 or newer is needed.

Binutils
--------

Binutils 2.30 or newer is needed to build the kernel.

pkg-config
----------

The build system, as of 4.18, requires pkg-config to check for installed
kconfig tools and to determine flags settings for use in
'make {g,x}config'.  Previously pkg-config was being used but not
verified or documented.

Flex
----

Since Linux 4.16, the build system generates lexical analyzers
during build.  This requires flex 2.5.35 or later.


Bison
-----

Since Linux 4.16, the build system generates parsers
during build.  This requires bison 2.0 or later.

pahole
------

Since Linux 5.2, if CONFIG_DEBUG_INFO_BTF is selected, the build system
generates BTF (BPF Type Format) from DWARF in vmlinux, a bit later from kernel
modules as well.  This requires pahole v1.16 or later.

It is found in the 'dwarves' or 'pahole' distro packages or from
https://fedorapeople.org/~acme/dwarves/.

Perl
----

You will need perl 5 and the following modules: ``Getopt::Long``,
``Getopt::Std``, ``File::Basename``, and ``File::Find`` to build the kernel.

BC
--

You will need bc to build kernels 3.10 and higher


OpenSSL
-------

Module signing and external certificate handling use the OpenSSL program and
crypto library to do key creation and signature generation.

You will need openssl to build kernels 3.7 and higher if module signing is
enabled.  You will also need openssl development packages to build kernels 4.3
and higher.

Tar
---

GNU tar is needed if you want to enable access to the kernel headers via sysfs
(CONFIG_IKHEADERS).

gtags / GNU GLOBAL (optional)
-----------------------------

The kernel build requires GNU GLOBAL version 6.6.5 or later to generate
tag files through ``make gtags``.  This is due to its use of the gtags
``-C (--directory)`` flag.

mkimage
-------

This tool is used when building a Flat Image Tree (FIT), commonly used on ARM
platforms. The tool is available via the ``u-boot-tools`` package or can be
built from the U-Boot source code. See the instructions at
https://docs.u-boot.org/en/latest/build/tools.html#building-tools-for-linux

GNU AWK
-------

GNU AWK is needed if you want kernel builds to generate address range data for
builtin modules (CONFIG_BUILTIN_MODULE_RANGES).

Kernel documentation
********************

Sphinx
------

Please see :ref:`sphinx_install` in :ref:`Documentation/doc-guide/sphinx.rst <sphinxdoc>`
for details about Sphinx requirements.

rustdoc
-------

``rustdoc`` is used to generate the documentation for Rust code. Please see
Documentation/rust/general-information.rst for more information.
