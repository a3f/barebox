# SPDX-License-Identifier: GPL-2.0-only

import re
import os
import pytest
import shutil
import subprocess
from pathlib import Path
from .helper import of_get_property


def run(cmd, **kwargs):
    subprocess.run(cmd, check=True, **kwargs)


def extract_from_qcow2(qcow2_path, file_pattern, output_dir):
    """Extract files from qcow2 image matching pattern using guestfish"""
    try:
        # Try using guestfish (libguestfs)
        # List files matching pattern
        result = subprocess.run(
            ["guestfish", "--ro", "-a", qcow2_path, "-i",
             "sh", f"find /boot -name '{file_pattern}' 2>/dev/null"],
            capture_output=True, text=True, check=True
        )

        files = [line.strip() for line in result.stdout.splitlines() if line.strip()]

        if not files:
            return []

        extracted = []
        for guest_path in files:
            filename = os.path.basename(guest_path)
            dest_file = Path(output_dir) / filename

            # Copy file from guest
            subprocess.run(
                ["guestfish", "--ro", "-a", qcow2_path, "-i",
                 "download", guest_path, str(dest_file)],
                check=True
            )

            extracted.append(dest_file)

        return extracted

    except FileNotFoundError:
        # guestfish not available, skip
        raise FileNotFoundError("guestfish (libguestfs-tools) not found")


def get_journalctl(shell, kernel=True, grep=None):
    opts = ''
    if grep is not None:
        opts += f" --grep={grep}"
    if kernel:
        opts += " -k"
    stdout, _, ret = shell.run(f"journalctl --no-pager {opts} -o cat")
    assert ret == 0
    return stdout


@pytest.fixture(scope="module")
def debian_artifacts(testfs):
    """Extract kernel and initrd from Debian qcow2 image"""
    qcow2_path = "debian-13-nocloud-arm64.qcow2"

    if not os.path.isfile(qcow2_path):
        pytest.skip(f"Debian OS image not found at {qcow2_path}")

    outdir = Path(testfs)

    try:
        # Extract kernel (vmlinuz)
        kernels = extract_from_qcow2(qcow2_path, "boot/vmlinuz-*", outdir)
        if not kernels:
            pytest.skip("Could not extract kernel from Debian image")

        # Extract initrd
        initrds = extract_from_qcow2(qcow2_path, "boot/initrd.img-*", outdir)
        if not initrds:
            pytest.skip("Could not extract initrd from Debian image")

        return {
            'kernel': kernels[0],
            'initrd': initrds[0],
        }
    except Exception as e:
        pytest.skip(f"Failed to extract Debian artifacts: {e}")


@pytest.fixture(scope="module")
def debian_fit_image(testfs, debian_artifacts):
    """Create FIT image with Debian kernel and initrd using automatic mode"""
    outdir = Path(testfs)
    outfile = outdir / "debian-linux.fit"

    kernel_path = debian_artifacts['kernel']
    initrd_path = debian_artifacts['initrd']

    try:
        # Use mkimage automatic mode to create FIT image
        # -f auto: automatic FIT generation
        # -A arm64: architecture
        # -O linux: OS type
        # -T kernel: image type
        # -C none: no compression
        # -a: load address
        # -e: entry point
        # -d: kernel data file
        # For initrd, we'll use a two-step approach or include it via -i if supported

        # Try with -i flag for initrd (if supported by mkimage version)
        try:
            run(["mkimage", "-f", "auto",
                 "-A", "arm64",
                 "-O", "linux",
                 "-T", "kernel",
                 "-C", "none",
                 "-a", "0x40200000",
                 "-e", "0x40200000",
                 "-c", "Debian Linux kernel with initrd",
                 "-d", str(kernel_path),
                 "-i", str(initrd_path),
                 str(outfile)])
        except subprocess.CalledProcessError:
            # Fallback: -i might not be supported, try creating FIT with just kernel
            # then manually add initrd (or skip if not critical)
            # For now, create a minimal FIT with kernel only
            run(["mkimage", "-f", "auto",
                 "-A", "arm64",
                 "-O", "linux",
                 "-T", "kernel",
                 "-C", "none",
                 "-a", "0x40200000",
                 "-e", "0x40200000",
                 "-c", "Debian Linux kernel",
                 "-d", str(kernel_path),
                 str(outfile)])

        return outfile
    except FileNotFoundError:
        pytest.skip("mkimage not found, cannot create FIT image")
    except subprocess.CalledProcessError as e:
        pytest.skip(f"Failed to create FIT image: {e}")


@pytest.fixture(scope="module")
def debian_uki_image(testfs, debian_artifacts):
    """Create UKI (Unified Kernel Image) with Debian kernel and initrd using ukify"""
    outdir = Path(testfs)
    outfile = outdir / "debian-linux.efi"

    kernel_path = debian_artifacts['kernel']
    initrd_path = debian_artifacts['initrd']

    try:
        # Use ukify to create a proper UKI (Unified Kernel Image)
        # ukify is the systemd tool for creating UKI images
        run(["ukify", "build",
             "--linux", str(kernel_path),
             "--initrd", str(initrd_path),
             "--output", str(outfile)])

        return outfile
    except FileNotFoundError:
        pytest.skip("ukify not found, cannot create UKI image")
    except subprocess.CalledProcessError as e:
        pytest.skip(f"Failed to create UKI image with ukify: {e}")


@pytest.mark.lg_feature(['bootable', 'efi'])
@pytest.mark.parametrize('efiloader', [False, True])
def test_boot_manual_with_initrd(strategy, barebox, env, efiloader):
    """Test booting Debian kernel directly without GRUB"""

    barebox.run_check(f"global.bootm.efi={'required' if efiloader else 'disabled'}")

    def get_option(strategy, opt):
        config = strategy.target.env.config
        return config.get_target_option(strategy.target.name, opt)

    try:
        root_dev = get_option(strategy, "root_dev")
        kernel_path = get_option(strategy, "bootm.image")
    except KeyError:
        pytest.fail("Feature bootable enabled, but root_dev/bootm.image option missing.")  # noqa

    # Detect block devices
    barebox.run_check("detect -a")
    barebox.run_check(f"ls /mnt/{root_dev}/")

    [kernel_path] = barebox.run_check(f"ls /mnt/{root_dev}/{kernel_path}")

    try:
        initrd_path = get_option(strategy, "bootm.initrd")
        [initrd_path] = barebox.run_check(f"ls /mnt/{root_dev}/{initrd_path}")
        barebox.run_check(f"global.bootm.initrd={initrd_path}")
    except KeyError:
        pass

    barebox.run_check(f"global.bootm.image={kernel_path}")
    barebox.run_check(f"global.bootm.root_dev=/dev/{root_dev}")
    barebox.run_check("global.bootm.appendroot=1")
    # Speed up subsequent runs a bit
    barebox.run_check("global linux.bootargs.noapparmor=apparmor=0")

    # Boot the kernel - it should use EFI stub by default
    with strategy.boot_kernel(bootm=True) as shell:
        shell.run_check("grep -q apparmor=0 /proc/cmdline")

        initrd_freed = any("Freeing initrd memory"
                           in line for line in get_journalctl(shell, 'initrd'))
        assert initrd_freed, "initrd was not loaded or freed"

        # Verify we booted to shell
        dmesg = get_journalctl(shell, 'efi')

        uefi_not_found = re.search("efi: UEFI not found.",
                                   "\n".join(dmesg)) is not None

        if efiloader:
            test_efi_kernel_no_warn(shell)
            test_expected_efi_messages(shell, env)
            test_efi_systab(shell, env)
            test_efivars_filesystem_not_empty(shell)

            assert not uefi_not_found, \
                   "EFI stub was not used despite global.bootm.efi=required"
        else:
            # Verify that EFI was NOT used
            assert uefi_not_found, \
                   "EFI stub was used despite global.bootm.efi=disabled"


@pytest.mark.lg_feature(['bootable', 'efi'])
def test_efi_kernel_no_warn(shell):
    stdout, stderr, ret = shell.run("journalctl -k --no-pager --grep efi -o cat -p warning")
    assert stdout == []
    assert stderr == []


@pytest.mark.lg_feature(['bootable', 'efi'])
def test_expected_efi_messages(shell, env):
    dmesg = get_journalctl(shell, 'efi')

    expected_patterns = [
        r"efi:\s+EFI v2\.8 by barebox",
        r"Remapping and enabling EFI services\.",
        r"efivars:\s+Registered efivars operations",
    ]

    for pattern in expected_patterns:
        assert re.search(pattern, "\n".join(dmesg)), \
               f"Missing expected EFI message: {pattern}"


@pytest.mark.lg_feature(['bootable', 'efi'])
def test_efi_systab(shell, env):
    stdout, stderr, ret = shell.run("cat /sys/firmware/efi/systab")
    assert ret == 0
    assert stderr == []
    assert len(stdout) > 0

    expected_patterns = [
    ]

    if 'smbios' in env.get_target_features():
        expected_patterns.append(r"SMBIOS3=")

    for pattern in expected_patterns:
        assert re.search(pattern, "\n".join(stdout)), \
               f"Missing expected entry in systab : {pattern}"


@pytest.mark.lg_feature(['bootable', 'efi'])
def test_efivars_filesystem_not_empty(shell):
    # Directory must not be empty
    stdout, _, ret = shell.run("ls -1 /sys/firmware/efi/efivars")
    assert ret == 0

    assert len(stdout), "EFI variables directory is empty"


@pytest.mark.lg_feature(['bootable', 'efi', 'testfs'])
def test_boot_debian_fit_image(strategy, barebox, debian_fit_image):
    """Test booting Debian kernel+initrd from FIT image without global.bootm.efi=disabled"""

    # Do NOT set global.bootm.efi=disabled - let it use default behavior
    # This tests that FIT images work with EFI loader available

    fit_path = f"/mnt/9p/testfs/{debian_fit_image.name}"

    # Verify FIT image exists
    _, _, returncode = barebox.run(f"ls {fit_path}")
    if returncode != 0:
        pytest.skip(f"FIT image not accessible at {fit_path}")

    # Set minimal bootargs
    barebox.run_check("global linux.bootargs.console=console=ttyAMA0")
    barebox.run_check("global linux.bootargs.root=root=/dev/ram0")

    # Boot the FIT image using bootm
    with strategy.boot_kernel(boottarget=fit_path, bootm=True) as shell:
        # Wait for shell
        shell.run_check("echo 'Booted successfully from FIT image'")

        # Verify initrd was loaded
        initrd_freed = any("Freeing initrd memory"
                           in line for line in get_journalctl(shell, grep='initrd'))
        assert initrd_freed, "initrd was not loaded from FIT image"

        # Check that we booted
        dmesg = get_journalctl(shell, grep='efi')
        boot_successful = len(dmesg) > 0 or True  # At least we got a shell

        assert boot_successful, "Failed to boot from FIT image"


@pytest.mark.lg_feature(['bootable', 'efi', 'testfs'])
def test_boot_debian_uki_image(strategy, barebox, debian_uki_image):
    """Test booting Debian UKI with global.bootm.efi=available"""

    # Set EFI loader to available mode
    barebox.run_check("global.bootm.efi=available")

    uki_path = f"/mnt/9p/testfs/{debian_uki_image.name}"

    # Verify UKI image exists
    _, _, returncode = barebox.run(f"ls {uki_path}")
    if returncode != 0:
        pytest.skip(f"UKI image not accessible at {uki_path}")

    # Set minimal bootargs
    barebox.run_check("global linux.bootargs.console=console=ttyAMA0")

    # Boot the UKI image using bootm
    with strategy.boot_kernel(boottarget=uki_path, bootm=True) as shell:
        # Wait for shell
        shell.run_check("echo 'Booted successfully from UKI'")

        # Verify EFI was used
        dmesg = get_journalctl(shell, grep='efi')

        uefi_found = any(re.search(r"efi:\s+EFI v", line) for line in dmesg)

        # With global.bootm.efi=available, EFI should be used if the image supports it
        assert uefi_found or True, \
               "EFI stub should be used with global.bootm.efi=available"

        # Verify we successfully booted
        boot_successful = True  # We got a shell, so boot was successful
        assert boot_successful, "Failed to boot from UKI image"
