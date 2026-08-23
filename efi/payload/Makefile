# SPDX-License-Identifier: GPL-2.0-only

obj-y += init.o
obj-y += image.o
obj-$(CONFIG_EFI_HANDOVER_PROTOCOL) += handover.o
obj-y += bootm.o
obj-$(CONFIG_OFTREE) += fdt.o
# Architectures with a dts directory build the fallback dtb there
ifeq ($(dtstree),)
obj-$(CONFIG_OFDEVICE) += fallback.dtb.o
endif
bbenv-y += env-efi
obj-$(CONFIG_CMD_IOMEM) += iomem.o
obj-pbl-$(CONFIG_EFI_PAYLOAD) += early-mem.o
obj-$(CONFIG_EFI_PAYLOAD) += entry-single.o
pbl-$(CONFIG_EFI_STUB) += entry-multi.o
obj-$(CONFIG_EFI_STUB) += boarddata.o
