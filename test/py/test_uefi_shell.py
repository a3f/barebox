# SPDX-License-Identifier: GPL-2.0-only
# Test UEFI Shell integration: barebox -> UEFI Shell -> Grub -> Linux

import re
import time

import pexpect
import pytest

from .helper import ensure_uefi_shell, ensure_debian_iso


fetchdir = "/mnt/9p/testfs"

# The UEFI Shell renders output via EFI SimpleTextOutput, which barebox
# translates to ANSI escape sequences with cursor positioning.  Every
# character may be preceded by color/position codes, so we need helpers
# to match and strip them.
_ANSI_RE = re.compile(r'\x1b\[[^a-zA-Z]*[a-zA-Z]')
_ANSI_ESC = r'(?:\x1b\[[^a-zA-Z]*[a-zA-Z])*'


def ansi_strip(text):
    """Remove ANSI escape sequences from text."""
    return _ANSI_RE.sub('', text)


def ansi_pattern(text):
    """Create regex matching text with optional ANSI escapes between chars."""
    return _ANSI_ESC + _ANSI_ESC.join(re.escape(c) for c in text)


@pytest.fixture(scope="module")
def uefi_shell(testfs):
    result = ensure_uefi_shell(testfs)
    if result is None:
        pytest.skip("UEFI Shell binary (shellaa64.efi) not found")
    return result


@pytest.fixture(scope="module")
def debian_iso(env, testfs):
    result = ensure_debian_iso(env, testfs)
    if result is None:
        pytest.skip("Debian ISO not found")
    return result


def efi_sendline(console, line):
    """Send a line to the UEFI Shell with CR termination.

    The UEFI Shell expects CR (\\r), not LF (\\n), as line terminator.
    """
    console.write(f"{line}\r".encode("utf-8"))


def efi_shell_cmd(console, cmd, timeout=30):
    """Send a command to the UEFI Shell and wait for the next Shell> prompt.

    Returns the ANSI-stripped output lines between command and prompt.
    """
    efi_sendline(console, cmd)
    _, before, _, _ = console.expect(ansi_pattern("Shell>"), timeout=timeout)
    if isinstance(before, bytes):
        before = before.decode("utf-8", errors="replace")
    return ansi_strip(before).splitlines()


@pytest.mark.lg_feature(['efi', 'testfs'])
def test_uefi_shell_commands(strategy, barebox, uefi_shell):  # noqa: ARG001
    """Test UEFI Shell basic commands: ver, map, filesystem navigation"""

    # Launch Shell.efi from barebox via bootm
    barebox.run_check(f"ls {fetchdir}/Shell.efi")

    console = strategy.console
    console.sendline(f"bootm {fetchdir}/Shell.efi")
    strategy.target.deactivate(strategy.barebox)

    try:
        # Wait for the Shell to start — initial prompt is plain text
        console.expect(r"Shell>", timeout=60)

        # Test 'ver' command - verify Shell version string
        output = efi_shell_cmd(console, "ver")
        ver_text = "\n".join(output)
        assert re.search(r"UEFI.*Shell", ver_text, re.IGNORECASE), \
            f"Shell version string not found in: {ver_text}"

        # Test 'map -r' command - verify device mappings visible
        output = efi_shell_cmd(console, "map -r")
        map_text = "\n".join(output)
        # Should show at least one FS or BLK mapping
        assert re.search(r"(FS\d+|BLK\d+)", map_text), \
            f"No FS/BLK mappings found in: {map_text}"

        # Test 'map -v' command - verify device path info
        output = efi_shell_cmd(console, "map -v")
        map_v_text = "\n".join(output)
        assert re.search(r"(VenHw|PciRoot|VirtioMmio|Acpi)", map_v_text,
                         re.IGNORECASE) or \
            re.search(r"(FS\d+|BLK\d+)", map_v_text), \
            f"No device path info found in: {map_v_text}"

        # Exit the Shell back to barebox
        efi_sendline(console, "exit")
    finally:
        strategy.power.cycle()
        strategy.target.activate(strategy.barebox)


@pytest.mark.lg_feature(['bootable', 'efi', 'testfs'])
def test_uefi_shell_grub_boot(strategy, barebox, env, uefi_shell, debian_iso):  # noqa: ARG001
    """Test full EFI chain: barebox -> UEFI Shell -> Grub -> Linux"""

    # Launch Shell.efi from barebox via bootm
    barebox.run_check(f"ls {fetchdir}/Shell.efi")

    console = strategy.console
    console.sendline(f"bootm {fetchdir}/Shell.efi")
    strategy.target.deactivate(strategy.barebox)

    try:
        # Wait for the Shell to start — initial prompt is plain text
        console.expect(r"Shell>", timeout=60)

        # Enumerate FS mappings to find Grub on the Debian ISO
        output = efi_shell_cmd(console, "map -r")
        map_text = "\n".join(output)

        # Find available FS mappings from stripped output
        fs_mappings = re.findall(r"(FS\d+)", map_text)
        # Deduplicate while preserving order
        seen = set()
        fs_mappings = [x for x in fs_mappings
                       if not (x in seen or seen.add(x))]

        grub_fs = None
        grub_path = r"EFI\boot\bootaa64.efi"

        for fs in fs_mappings:
            # Check each FS for Grub by listing the expected path
            output = efi_shell_cmd(console, f"{fs}:ls {grub_path}")
            if re.search(r"bootaa64\.efi", "\n".join(output),
                         re.IGNORECASE):
                grub_fs = fs
                break

        assert grub_fs is not None, \
            f"Grub not found on any FS mapping. Checked: {fs_mappings}"

        # Execute Grub from the UEFI Shell using full path
        efi_sendline(console, f"{grub_fs}:{grub_path}")

        # Handle Grub menu and wait for installer boot.
        # Send Enter periodically to dismiss GRUB menu and installer
        # prompts, matching the pattern in skip_cdrom_installer().
        from labgrid.util.timeout import Timeout

        timeout = Timeout(float(strategy.shell.login_timeout))

        while not timeout.expired:
            console.sendline("")
            index, _, _, _ = console.expect(
                ["Starting system log daemon", pexpect.TIMEOUT],
                timeout=strategy.shell.await_login_timeout)
            if index == 0:
                break

        assert not timeout.expired, \
            "Timed out waiting for Debian installer to boot"

        # Switch to screen window 3 (shell) and disable the status bar.
        # Same pattern as strategy.skip_cdrom_installer().
        while not timeout.expired:
            console.sendline("\x013")
            time.sleep(0.5)
            console.sendline("\x01:hardstatus ignore")
            time.sleep(0.5)
            console.sendline("")
            index, _, _, _ = console.expect(
                [strategy.shell.prompt, pexpect.TIMEOUT],
                timeout=strategy.shell.await_login_timeout)
            if index == 0:
                break

        assert not timeout.expired, "Timed out waiting for Linux shell"

        # Activate ShellDriver for structured interaction
        strategy.target.activate(strategy.shell)

        try:
            # Verify kernel booted
            stdout, _, ret = strategy.shell.run("uname -r")
            assert ret == 0, "uname failed - kernel did not boot"

            # Verify EFI runtime is available
            _, _, ret = strategy.shell.run("test -d /sys/firmware/efi")
            assert ret == 0, \
                "/sys/firmware/efi not found - EFI runtime not available"

            # Check dmesg for barebox EFI messages
            stdout, _, ret = strategy.shell.run(
                "dmesg | grep -i 'efi.*barebox'")
            assert ret == 0, "No barebox EFI messages in dmesg"
            assert len(stdout) > 0, "Empty barebox EFI dmesg output"
        finally:
            strategy.target.deactivate(strategy.shell)
    finally:
        strategy.power.cycle()
        strategy.target.activate(strategy.barebox)
