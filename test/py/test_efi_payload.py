# SPDX-License-Identifier: GPL-2.0-only

import pytest

from .helper import skip_disabled

# EFI_BAREBOX_VENDOR_GUID
BAREBOX_GUID = "5b91f69c-8b88-4a2b-9269-5f1d802b5175"
FDT_MAGIC = ["d0", "0d", "fe", "ed"]

CHAINLOAD_IMAGES = ["/dev/fw_cfg.kernel",
                    "/boot/EFI/BOOT/BOOTAA64.EFI",
                    "/boot/EFI/BOOT/BOOTX64.EFI"]


def md_bytes(barebox, path, offset, count):
    """Return count bytes read at offset from path as a list of hex strings"""
    out = barebox.run_check(f"md -b -s {path} {offset}+{count}")
    dump = []
    for line in out:
        # strip the address prefix and stop before the ASCII column
        tokens = line.split(":", 1)[1].split()
        dump += tokens[:min(16, count - len(dump))]
    return dump


def find_chainload_image(barebox):
    for image in CHAINLOAD_IMAGES:
        _, _, ret = barebox.run(f"ls {image}")
        if ret == 0:
            return image
    return None


def test_efi_devicetree_variable(strategy, barebox, barebox_config):
    """barebox as EFI payload exports its device tree for the OS to find"""
    skip_disabled(barebox_config, "CONFIG_EFI_PAYLOAD", "CONFIG_FS_EFIVARFS",
                  "CONFIG_OFTREE", "CONFIG_CMD_MD")

    dtb = f"/efivarfs/barebox-dtb-{BAREBOX_GUID}"

    barebox.run_check("mkdir -p /efivarfs")
    barebox.run_check("mount -t efivarfs none /efivarfs")

    # the device tree is exported just before booting, so not yet
    _, _, ret = barebox.run(f"ls {dtb}")
    assert ret != 0, "barebox-dtb variable already set before boot"

    image = find_chainload_image(barebox)
    if image is None:
        pytest.skip("no EFI image found to chainload")

    try:
        # chainload ourselves to make barebox export the device tree
        strategy.console.sendline(f"bootm {image}")
        strategy.target.deactivate(strategy.barebox)
        strategy.target.activate(strategy.barebox)
        barebox = strategy.barebox

        barebox.run_check("mkdir -p /efivarfs")
        barebox.run_check("mount -t efivarfs none /efivarfs")

        header = md_bytes(barebox, dtb, 0, 8)
        assert header[:4] == FDT_MAGIC

        # the whole blob must have made it into the variable
        totalsize = int("".join(header[4:8]), 16)
        assert md_bytes(barebox, dtb, totalsize - 4, 4)
    finally:
        # boot a pristine barebox for the tests that follow
        strategy.target.deactivate(strategy.barebox)
        strategy.power.cycle()
        strategy.target.activate(strategy.barebox)
