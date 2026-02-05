# SPDX-License-Identifier: GPL-2.0-only

import pytest

from .helper import ensure_debian_vfat_image


@pytest.fixture(scope="module", autouse=True)
def debian_vfat_image():
    yield ensure_debian_vfat_image()


@pytest.fixture(scope="function")
def shell(strategy, barebox):
    """Boot Linux and provide shell access for SMBIOS tests."""
    ensure_debian_vfat_image()

    def get_option(opt):
        config = strategy.target.env.config
        return config.get_target_option(strategy.target.name, opt)

    root_dev = get_option("root_dev")
    kernel_path = get_option("bootm.image")

    barebox.run_check("detect -a")
    barebox.run_check(f"ls /mnt/{root_dev}/")
    kernel_path = barebox.run_check(f"ls /mnt/{root_dev}/{kernel_path}")[0]

    try:
        initrd_path = get_option("bootm.initrd")
        initrd_path = barebox.run_check(f"ls /mnt/{root_dev}/{initrd_path}")[0]
        barebox.run_check(f"global.bootm.initrd={initrd_path}")
    except KeyError:
        pass

    barebox.run_check(f"global.bootm.image={kernel_path}")
    barebox.run_check(f"global.bootm.root_dev=/dev/{root_dev}")
    barebox.run_check("global.bootm.appendroot=1")
    barebox.run_check("global.bootm.efi=required")

    with strategy.boot_kernel(bootm=True) as sh:
        yield sh


@pytest.mark.lg_feature(['bootable', 'smbios'])
def test_smbios3_tables_present(shell):
    _, _, ret = shell.run("test -e /sys/firmware/dmi/tables/smbios_entry_point")
    assert ret == 0, "SMBIOS entry point not found"

    [stdout], _, ret = shell.run("wc -c </sys/firmware/dmi/tables/DMI")
    assert ret == 0

    size = int(stdout)
    assert size > 0, "SMBIOS DMI table is empty"

    shell.run_check("echo _SM3_ >/tmp/sm3")
    stdout, _, ret = shell.run("cmp --bytes 5 /tmp/sm3 /sys/firmware/dmi/tables/smbios_entry_point")
    assert stdout == []
    assert ret == 0, "SMBIOS entry point is not SMBIOS 3.x"


@pytest.mark.lg_feature(['bootable', 'smbios'])
def test_smbios_contains_barebox(shell):
    """
    Search raw SMBIOS/DMI tables for a barebox vendor string.
    This avoids dmidecode and relies on simple string matching.
    """
    # The DMI table is binary; strings are still ASCII embedded
    stdout, _, ret = shell.run("grep -a barebox /sys/firmware/dmi/tables/DMI")
    assert len(stdout) > 0, "barebox not found in SMBIOS/DMI tables"
