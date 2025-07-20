# SPDX-License-Identifier: GPL-2.0-or-later

import pytest
from .fixups import BareboxNullPointerAbort, BareboxDataAbort, \
                    BareboxGuardPageAbort
from .helper import skip_disabled, deep_lookup


@pytest.fixture(autouse=True)
def mmu_test_deps(barebox_arm, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_MD", "CONFIG_CMD_MW")


def getaddr(iomem, key):
    obj = next(deep_lookup(iomem, key), None)
    return None if obj is None else obj['start']


def read_pagefaults(barebox, addr):
    dump = barebox.run_check(f"md -b {addr}+1")
    if "xx" in dump[0]:
        return True
    if len(dump) == 1:
        return False
    raise Exception(f"Unexpected md output: {dump}")


def write_pagefaults(barebox, addr, exception):
    # Print a notice, so the splats in the console log is not misunderstood
    barebox.run_check("echo Test if illegal write crashes")
    with pytest.raises(exception):
        barebox.run_check(f"mw {addr} 0")
    return True


def assert_rw_pagefaults(barebox, addr, write_exception):
    assert read_pagefaults(barebox, addr)
    assert write_pagefaults(barebox, addr, write_exception)


def test_zero_page(barebox):
    assert_rw_pagefaults(barebox, 0, BareboxNullPointerAbort)


def test_guard_page(barebox, barebox_iomem):
    guardpage = getaddr(barebox_iomem, 'guard page')
    if guardpage is None:
        pytest.skip("no guard page in iomem")

    assert_rw_pagefaults(barebox, guardpage, BareboxGuardPageAbort)


def test_mmu_permissions(barebox, barebox_config, barebox_iomem):
    skip_disabled(barebox_config, "CONFIG_ARM_MMU_PERMISSIONS")

    code_addr = getaddr(barebox_iomem, "barebox code")
    assert not read_pagefaults(barebox, code_addr)
    assert write_pagefaults(barebox, code_addr, BareboxDataAbort)

    ro_addr = getaddr(barebox_iomem, "barebox RO data")
    assert not read_pagefaults(barebox, ro_addr)
    assert write_pagefaults(barebox, ro_addr, BareboxDataAbort)

    data_addr = getaddr(barebox_iomem, "barebox data")
    assert not read_pagefaults(barebox, data_addr)

    bss_addr = getaddr(barebox_iomem, "barebox bss")
    assert not read_pagefaults(barebox, bss_addr)


def test_mmu_optee_uncached(barebox, barebox_config, barebox_iomem):
    skip_disabled(barebox_config, "CONFIG_HAVE_OPTEE", "CONFIG_MMUINFO")

    optee_core = next(deep_lookup(barebox_iomem, "optee_core"), None)

    assert optee_core is not None, "optee_core region not found"
    assert optee_core['reserved'] is True

    # TODO: once iomem has been taught to walk the page tables manually,
    # when invoked without arguments, use that instead
    mmuinfo = barebox.run_check(f"mmuinfo {optee_core['start']}")

    rw = 0
    for line in mmuinfo:
        # ARM64
        if "Translation aborted" in line:
            rw = rw + 1

        if "Memory attr. [63:56]:" in line:
            assert "0x00 (0b0000 Device-nGnRnE memory)" in line
            rw = rw + 1

        # ARM32
        if "Failure [0]:              0x1":
            rw = rw + 1

        if "Inner mem. attr. [6:4]:" in line:
            assert "0x0 (0b000 Non-cacheable)" in line

        if "Outer mem. attr. [3:2]:" in line:
            assert "0x0 (0b00 Non-cacheable)" in line
            rw = rw + 1

    assert rw >= 2, f"No Memory attr. found in mmuinfo output: {mmuinfo}"
