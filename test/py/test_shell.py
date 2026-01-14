# SPDX-License-Identifier: GPL-2.0-only

from .helper import skip_disabled
import json
import pytest
import re


def test_barebox_true(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_TRUE")

    _, _, returncode = barebox.run('true')
    assert returncode == 0


def test_barebox_false(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_FALSE")

    _, _, returncode = barebox.run('false')
    assert returncode == 1


def test_barebox_md5sum(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_MD5SUM", "CONFIG_CMD_ECHO")

    barebox.run_check("echo -o md5 test")
    out = barebox.run_check("md5sum md5")
    assert out == ["d8e8fca2dc0f896fd7cb4cb0031ba249  md5"]


def test_barebox_version(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_VERSION")

    stdout, _, returncode = barebox.run('version')
    assert 'barebox' in stdout[1]
    assert returncode == 0


def test_barebox_no_err(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_DMESG")

    # TODO extend by err once all qemu platforms conform
    stdout, _, _ = barebox.run('dmesg -l crit,alert,emerg')
    assert stdout == []


def count_dicts_in_command_output(barebox, cmd):
    def count_dicts(obj):
        count = 0
        if isinstance(obj, dict):
            count += 1  # count this dict itself
            for value in obj.values():
                count += count_dicts(value)
        elif isinstance(obj, list):
            for item in obj:
                count += count_dicts(item)
        return count

    stdout = "\n".join(barebox.run_check(cmd))
    return count_dicts(json.loads(stdout))


def test_cmd_iomem(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_IOMEM")

    regions = count_dicts_in_command_output(barebox, 'iomem -j')
    assert regions > 0

    assert count_dicts_in_command_output(barebox, 'iomem -jv') == regions
    if regions > 1:
        assert count_dicts_in_command_output(barebox, 'iomem -jg') > regions
        assert count_dicts_in_command_output(barebox, 'iomem -vjg') > regions
    else:
        assert count_dicts_in_command_output(barebox, 'iomem -jg') >= regions
        assert count_dicts_in_command_output(barebox, 'iomem -vjg') >= regions


def test_cmd_clk(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_CLK")

    regions = count_dicts_in_command_output(barebox, 'clk_dump -j')
    assert regions >= 0

    assert count_dicts_in_command_output(barebox, 'clk_dump -vj') == regions


def test_cmd_addpart(barebox, barebox_config):
    skip_disabled(barebox_config, "CONFIG_CMD_PARTITION", "CONFIG_CMD_IOMEM")

    def find_first_malloc_space(node, parent=None):
        if node.get("name") == "malloc space" and parent is not None:
            start = int(node["start"], 16)
            offset = start - int(parent["start"], 16)

            return parent["name"], offset

        for child in node.get("children", []):
            result = find_first_malloc_space(child, node)
            if result is not None:
                return result

        return None

    barebox.run_check('ls /dev/ram0')

    iomem = json.loads("\n".join(barebox.run_check("iomem -j")))
    assert iomem is not None

    parent, start = find_first_malloc_space(iomem)
    if parent is None:
        pytest.skip("No malloc space in iomem output")

    cmd = f"addpart /dev/{parent} 4K@0x{start:08x}(test-partition)Ro"
    barebox.run_check(cmd)

    testpartition = f"/dev/{parent}.test-partition"

    _, _, returncode = barebox.run(f"[ -e {testpartition} ]")
    assert returncode == 0, f"{cmd} did not add the partition"

    hexdump = barebox.run_check(f"md -b -s /dev/{parent}.test-partition 4091")
    assert re.fullmatch(r'00000ffb:(?:\s[0-9a-fA-F]{2}){5}\s{37}.{5}', hexdump[0]), \
           f"expected exactly 5 bytes in hexdump, but got: {hexdump[0]}"
    assert len(hexdump) == 1

    cmd = f"delpart {testpartition}"
    barebox.run_check(cmd)

    _, _, returncode = barebox.run(f"[ -e {testpartition} ]")
    assert returncode != 0, f"{cmd} did not deleete the partition"
