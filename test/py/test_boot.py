# SPDX-License-Identifier: GPL-2.0-or-later

import pytest
import os
import subprocess
from pathlib import Path
from .helper import skip_disabled, of_get_property

image_path = "/mnt/9p/testfs/test-image.bin"
initrd_path = "/mnt/9p/testfs/test-initrd.bin"

@pytest.fixture(scope="function")
def bootm_mock(barebox):
    barebox.run_check("global bootm.mock=1")
    yield
    barebox.run_check("global bootm.mock=0")


@pytest.fixture(autouse=True)
def p9_net_set_fetchdir(barebox):
    [old] = barebox.run_check("echo $global.net.fetchdir")
    barebox.run_check("global.net.fetchdir=/mnt/9p/testfs")
    yield
    barebox.run_check(f"global.net.fetchdir={old}")
    

@pytest.fixture(scope="function")
def boot_testdata(barebox_config, testfs):
    skip_disabled(barebox_config, "CONFIG_BOOTM_MOCK")

    outdir = Path(testfs)

    # Create a dummy boot image (just some recognizable data)
    image_data = b"#!/bin/sh\n# TEST IMAGE" + bytes(range(256)) * 16
    (outdir / "test-image.bin").write_bytes(image_data)

    # Create a dummy initrd (cpio-like header for recognizability)
    initrd_data = b"#!/bin/sh\n# TEST INITRD" + bytes(range(256)) * 16
    (outdir / "test-initrd.bin").write_bytes(initrd_data)

    return {
        "image": outdir / "test-image.bin",
        "initrd": outdir / "test-initrd.bin",
        "image_data": image_data,
        "initrd_data": initrd_data,
    }


@pytest.fixture(scope="module")
def compile_dtb():
    """Fixture that compiles DTS source to DTB using dtc"""

    # Get the build directory from environment
    builddir = os.environ.get('LG_BUILDDIR')
    if not builddir:
        pytest.skip("LG_BUILDDIR not set")

    dtc_path = Path(builddir) / "scripts" / "dtc" / "dtc"
    if not dtc_path.exists():
        pytest.skip(f"dtc not found at {dtc_path}")

    def _compile(outdir, dts_content, output_name):
        """Compile DTS to DTB

        Args:
            outdir: Directory where the DTB should be written
            dts_content: String containing device tree source
            output_name: Name for the output DTB file (without path)

        Returns:
            Path to the compiled DTB file
        """
        # Write DTS to temporary file
        dts_file = Path(outdir) / f"{output_name}.dts"
        dts_file.write_text(dts_content)

        # Compile to DTB
        dtb_file = Path(outdir) / output_name
        subprocess.run(
            [str(dtc_path), "-I", "dts", "-O", "dtb", "-o", str(dtb_file), str(dts_file)],
            check=True,
            capture_output=True
        )

        # Clean up DTS file
        dts_file.unlink()

        return dtb_file

    return _compile


@pytest.fixture(scope="module")
def make_fit_image():
    """Fixture that creates FIT images for testing"""

    builddir = os.environ.get('LG_BUILDDIR')
    if not builddir:
        pytest.skip("LG_BUILDDIR not set")

    def _make_fit(outdir, image_data, output_name, initrd_data=None, dtb_data=None):
        """Create a FIT image with kernel and optionally initrd/dtb

        Args:
            outdir: Directory where the FIT image should be written
            image_data: Bytes to use as kernel image
            output_name: Name for the output FIT file (without path)
            initrd_data: Optional bytes to use as initrd
            dtb_data: Optional bytes to use as device tree

        Returns:
            Path to the created FIT image file
        """
        outdir = Path(outdir)
        builddir_path = Path(builddir)

        # Write kernel data
        kernel_file = builddir_path / "fit-kernel.bin"
        kernel_file.write_bytes(image_data)

        # Build ITS content
        its_content = """/dts-v1/;
/ {
    description = "Test FIT image";
    #address-cells = <1>;
    images {
        kernel {
            description = "Test kernel";
            data = /incbin/("fit-kernel.bin");
            type = "kernel";
            arch = "arm64";
            os = "linux";
            compression = "none";
            hash-1 {
                algo = "sha256";
            };
        };
"""

        # Add initrd if provided
        if initrd_data is not None:
            initrd_file = builddir_path / "fit-initrd.bin"
            initrd_file.write_bytes(initrd_data)
            its_content += """        ramdisk {
            description = "Test initrd";
            data = /incbin/("fit-initrd.bin");
            type = "ramdisk";
            arch = "arm64";
            os = "linux";
            compression = "none";
            hash-1 {
                algo = "sha256";
            };
        };
"""

        # Add dtb if provided
        if dtb_data is not None:
            dtb_file = builddir_path / "fit-fdt.dtb"
            dtb_file.write_bytes(dtb_data)
            its_content += """        fdt {
            description = "Test device tree";
            data = /incbin/("fit-fdt.dtb");
            type = "flat_dt";
            arch = "arm64";
            compression = "none";
            hash-1 {
                algo = "sha256";
            };
        };
"""

        # Close images section and add configuration
        its_content += """    };
    configurations {
        default = "conf-1";
        conf-1 {
            description = "Test configuration";
            kernel = "kernel";
"""
        if initrd_data is not None:
            its_content += """            ramdisk = "ramdisk";
"""
        if dtb_data is not None:
            its_content += """            fdt = "fdt";
"""
        its_content += """        };
    };
};
"""

        # Write ITS file
        its_file = builddir_path / "test.its"
        its_file.write_text(its_content)

        # Create FIT image
        fit_file = outdir / output_name
        try:
            subprocess.run(
                ["mkimage", "-f", str(its_file), str(fit_file)],
                check=True,
                capture_output=True,
                cwd=str(builddir_path)
            )
        except FileNotFoundError:
            pytest.skip("mkimage not found")
        except subprocess.CalledProcessError as e:
            pytest.skip(f"mkimage failed: {e.stderr.decode()}")

        # Clean up temporary files
        kernel_file.unlink(missing_ok=True)
        if initrd_data is not None and 'initrd_file' in locals():
            initrd_file.unlink(missing_ok=True)
        if dtb_data is not None and 'dtb_file' in locals():
            dtb_file.unlink(missing_ok=True)
        its_file.unlink(missing_ok=True)

        return fit_file

    return _make_fit


def hashsum(barebox, file):
    # Verify the image content matches
    [image_hash] = barebox.run_check(f"md5sum {file}")
    return image_hash.split()[0]


def test_bootm_mock_handler(barebox, bootm_mock, boot_testdata):  # noqa: ARG001
    """Test that the test image handler writes boot data to /tmp/lastboot/"""

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Run bootm with our test image and initrd
    _, _, ret = barebox.run(f"bootm -r {initrd_path} {image_path}")

    # The test handler should succeed (return 0)
    assert ret == 0, "bootm failed"

    # Check that files were written to /tmp/lastboot/
    barebox.run_check("test -d /tmp/lastboot")
    barebox.run_check("test -f /tmp/lastboot/image")
    barebox.run_check("test -f /tmp/lastboot/initrd")

    assert hashsum(barebox, image_path) == hashsum(barebox, "/tmp/lastboot/image")

    barebox.run_check("mw -l -d /tmp/lastboot/image 0+1 0xdeadbeef")

    [image_hash1] = barebox.run_check(f"md5sum {image_path}")
    image_hash1 = image_hash1.split()[0]
    [image_hash2] = barebox.run_check("md5sum /tmp/lastboot/image")
    image_hash2 = image_hash2.split()[0]
    assert image_hash1 != image_hash2, "Hash sanity check failed"

    assert hashsum(barebox, initrd_path) == hashsum(barebox, "/tmp/lastboot/initrd")


def test_bootm_mock_handler_disabled(barebox, boot_testdata):  # noqa: ARG001
    """Test that the test handler doesn't interfere when disabled"""

    # Ensure the test image handler is disabled
    barebox.run_check("global bootm.mock=0")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Without the test handler, bootm should fail since we have an unknown filetype
    _, _, ret = barebox.run(f"bootm {image_path}")

    # Should fail because no handler matches
    assert ret != 0, "bootm should fail without test handler enabled"

    # /tmp/lastboot should not exist
    _, _, ret = barebox.run("test -d /tmp/lastboot")
    assert ret != 0, "/tmp/lastboot should not exist when handler is disabled"


def test_bootm_mock_dryrun(barebox, bootm_mock, boot_testdata):  # noqa: ARG001
    """Test that dryrun mode doesn't write files"""

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Run bootm with dryrun flag
    _, _, ret = barebox.run(f"bootm -d -f -r {initrd_path} {image_path}")

    assert ret == 0, "bootm dryrun failed"

    # /tmp/lastboot should not exist in dryrun mode
    _, _, ret = barebox.run("test -d /tmp/lastboot")
    assert ret != 0, "/tmp/lastboot should not exist in dryrun mode"


def test_bootm_initrd_dt_properties(barebox, bootm_mock, boot_testdata):
    """Test that the DT contains correct linux,initrd-start/end properties"""

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Get the initrd file size
    initrd_size = len(boot_testdata["initrd_data"])

    # Run bootm
    _, _, ret = barebox.run(f"bootm -r {initrd_path} {image_path}")
    assert ret == 0, "bootm failed"

    # Check that oftree was written
    barebox.run_check("test -f /tmp/lastboot/oftree")

    # Read the initrd properties from the saved DTB file using of_dump -f
    # ncells=0 means combine all cells into one big integer
    initrd_start = of_get_property(barebox, "/chosen/linux,initrd-start",
            file="/tmp/lastboot/oftree", ncells=0)
    initrd_end = of_get_property(barebox, "/chosen/linux,initrd-end",
            file="/tmp/lastboot/oftree", ncells=0)

    assert initrd_start is not False, "linux,initrd-start property not found"
    assert initrd_end is not False, "linux,initrd-end property not found"

    # end is exclusive, so end - start = size
    actual_size = initrd_end - initrd_start
    assert actual_size == initrd_size, \
        f"Initrd size mismatch: DT says {actual_size}, file is {initrd_size}"


def test_boot_script_with_bootm(barebox, bootm_mock, boot_testdata):  # noqa: ARG001
    """Test boot script that sets bootm variables and calls bootm"""

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create a boot script that sets the bootm variables and calls bootm
    # The script's bootm call will succeed with test handler, but then
    # boot command will try to boot again (since we didn't actually boot).
    # We just need to verify the test handler was called successfully.
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testscript '#!/bin/sh'")
    barebox.run_check("echo -a /env/boot/testscript 'global.bootm.image=/mnt/9p/testfs/test-image.bin'")
    barebox.run_check("echo -a /env/boot/testscript 'global.bootm.initrd=/mnt/9p/testfs/test-initrd.bin'")

    # Run the boot script - ignore return code since boot command may fail
    # after our test handler succeeds (it tries to boot again)
    barebox.run("boot testscript")

    # Verify files were written by the test handler

    assert hashsum(barebox, image_path) == hashsum(barebox, "/tmp/lastboot/image")
    assert hashsum(barebox, initrd_path) == hashsum(barebox, "/tmp/lastboot/initrd")

    # Clean up
    barebox.run("rm /env/boot/testscript")


def test_boot_script_without_bootm(barebox, bootm_mock, boot_testdata):  # noqa: ARG001
    """Test boot script that only sets bootm variables (boot command calls bootm)"""

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create a boot script that only sets the variables (no explicit bootm call)
    # The boot command should automatically call bootm after the script
    # We need to also set global.bootm.force to make bootm accept unknown filetypes
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testscript2 '#!/bin/sh'")
    barebox.run_check("echo -a /env/boot/testscript2 'global.bootm.image=/mnt/9p/testfs/test-image.bin'")
    barebox.run_check("echo -a /env/boot/testscript2 'global.bootm.initrd=/mnt/9p/testfs/test-initrd.bin'")

    # Run the boot script
    barebox.run("boot testscript2")

    # Verify files were written
    assert hashsum(barebox, image_path) == hashsum(barebox, "/tmp/lastboot/image")
    assert hashsum(barebox, initrd_path) == hashsum(barebox, "/tmp/lastboot/initrd")

    # Clean up
    barebox.run("rm /env/boot/testscript2")


def test_blspec_entry(barebox, barebox_config, bootm_mock, boot_testdata):  # noqa: ARG001
    """Test bootloader spec entry"""

    skip_disabled(barebox_config, "CONFIG_BLSPEC")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create a blspec entry directory structure
    barebox.run_check("mkdir -p /mnt/9p/testfs/loader/entries")

    # Create a blspec conf file with absolute paths
    barebox.run_check("echo -o /mnt/9p/testfs/loader/entries/test.conf 'title Test Entry'")
    barebox.run_check("echo -a /mnt/9p/testfs/loader/entries/test.conf 'linux test-image.bin'")
    barebox.run_check("echo -a /mnt/9p/testfs/loader/entries/test.conf 'initrd test-initrd.bin'")

    # Boot using the blspec entry with -f to force bootm
    # Ignore return code as boot may try to boot again after test handler succeeds
    barebox.run("boot /mnt/9p/testfs/loader/entries/test.conf")

    # Verify files were written by the test handler
    assert hashsum(barebox, image_path) == hashsum(barebox, "/tmp/lastboot/image")
    assert hashsum(barebox, initrd_path) == hashsum(barebox, "/tmp/lastboot/initrd")

    # Clean up
    barebox.run("rm -rf /mnt/9p/testfs/loader")


# Override functionality tests


def test_boot_override_initrd(barebox, barebox_config, bootm_mock, boot_testdata):  # noqa: ARG001
    """Test that -o bootm.initrd override works"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create an alternate initrd file
    outdir = Path(boot_testdata["initrd"].parent)
    alt_initrd_data = b"#!/bin/sh\n# ALTERNATE INITRD" + bytes(range(256)) * 16
    (outdir / "test-initrd-alt.bin").write_bytes(alt_initrd_data)

    alt_initrd_path = "/mnt/9p/testfs/test-initrd-alt.bin"

    # Create a boot script that uses the default initrd
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testoverride '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/testoverride 'global.bootm.image={image_path}'")
    barebox.run_check(f"echo -a /env/boot/testoverride 'global.bootm.initrd={initrd_path}'")

    # Run boot with override for initrd
    barebox.run(f"boot -o bootm.initrd={alt_initrd_path} testoverride")

    # Verify the alternate initrd was used
    barebox.run_check("test -f /tmp/lastboot/initrd")
    assert hashsum(barebox, alt_initrd_path) == hashsum(barebox, "/tmp/lastboot/initrd"), \
        "Override initrd was not used"

    # Verify image was still the default
    assert hashsum(barebox, image_path) == hashsum(barebox, "/tmp/lastboot/image")

    # Clean up
    barebox.run("rm /env/boot/testoverride")
    barebox.run(f"rm {alt_initrd_path}")


def test_boot_override_oftree(barebox, barebox_config, bootm_mock, boot_testdata, compile_dtb, testfs):  # noqa: ARG001
    """Test that -o bootm.oftree override works"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create an alternate device tree file with a distinctive model property
    expected_model = "Test Override DT"
    alt_oftree_dts = f"""/dts-v1/;
/ {{
    model = "{expected_model}";
}};
"""
    compile_dtb(testfs, alt_oftree_dts, "test-oftree-alt.dtb")

    alt_oftree_path = "/mnt/9p/testfs/test-oftree-alt.dtb"

    # Create a boot script
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testoverride2 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/testoverride2 'global.bootm.image={image_path}'")

    # Run boot with override for oftree
    barebox.run(f"boot -o bootm.oftree={alt_oftree_path} testoverride2")

    # Verify the override oftree was used by checking the model property
    # We can't use hash comparison because bootm applies fixups to the DT
    barebox.run_check("test -f /tmp/lastboot/oftree")

    # Read the model property from the fixed-up device tree
    actual_model = of_get_property(barebox, "/model",
            file="/tmp/lastboot/oftree")

    assert expected_model == actual_model

    # Verify image was still used
    assert hashsum(barebox, image_path) == hashsum(barebox, "/tmp/lastboot/image")

    # Clean up
    barebox.run("rm /env/boot/testoverride2")
    barebox.run(f"rm {alt_oftree_path}")


def test_boot_override_image(barebox, barebox_config, bootm_mock, boot_testdata):  # noqa: ARG001
    """Test that -o bootm.image override works"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create an alternate image file
    outdir = Path(boot_testdata["image"].parent)
    alt_image_data = b"#!/bin/sh\n# ALTERNATE IMAGE" + bytes(range(256)) * 16
    (outdir / "test-image-alt.bin").write_bytes(alt_image_data)

    alt_image_path = "/mnt/9p/testfs/test-image-alt.bin"

    # Create a boot script that uses the default image
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testoverride3 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/testoverride3 'global.bootm.image={image_path}'")

    # Run boot with override for image
    barebox.run(f"boot -o bootm.image={alt_image_path} testoverride3")

    # Verify the alternate image was used
    barebox.run_check("test -f /tmp/lastboot/image")
    assert hashsum(barebox, alt_image_path) == hashsum(barebox, "/tmp/lastboot/image"), \
        "Override image was not used"

    # Clean up
    barebox.run("rm /env/boot/testoverride3")
    barebox.run(f"rm {alt_image_path}")


# devboot functionality tests


@pytest.fixture(scope="function")
def devboot_testdata(barebox_config, testfs, boot_testdata, compile_dtb):
    """Prepare test files for devboot tests"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    outdir = Path(testfs)

    # Create alternate files for devboot overrides
    alt_image_data = b"#!/bin/sh\n# DEVBOOT IMAGE" + bytes(range(256)) * 16
    (outdir / "devboot-image.bin").write_bytes(alt_image_data)

    alt_initrd_data = b"#!/bin/sh\n# DEVBOOT INITRD" + bytes(range(256)) * 16
    (outdir / "devboot-initrd.bin").write_bytes(alt_initrd_data)

    # Create device tree files with distinctive models
    devboot_oftree_dts = """/dts-v1/;
/ {
    model = "Devboot Script DT";
};
"""
    compile_dtb(testfs, devboot_oftree_dts, "devboot-oftree.dtb")

    # Create an alternate device tree for command-line override testing
    cmdline_oftree_dts = """/dts-v1/;
/ {
    model = "Command Line Override DT";
};
"""
    compile_dtb(testfs, cmdline_oftree_dts, "cmdline-oftree.dtb")

    return {
        "devboot_image": outdir / "devboot-image.bin",
        "devboot_initrd": outdir / "devboot-initrd.bin",
        "devboot_oftree": outdir / "devboot-oftree.dtb",
        "cmdline_oftree": outdir / "cmdline-oftree.dtb",
        "alt_image_data": alt_image_data,
        "alt_initrd_data": alt_initrd_data,
    }


def test_devboot_script_arch(barebox, bootm_mock, boot_testdata, devboot_testdata):  # noqa: ARG001
    """Test devboot with arch-specific script"""

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")
    barebox.run("rm /mnt/9p/testfs/*-devboot-*")

    # Get global variables
    [user] = barebox.run_check("echo $global.user")
    [arch] = barebox.run_check("echo $global.arch")

    script_path = f"/mnt/9p/testfs/{user}-devboot-{arch}"

    barebox.run_check(f"echo -o {script_path} '#!/bin/sh'")
    barebox.run_check(f"echo -a {script_path} '# devboot configuration script'")
    barebox.run_check(f"echo -a {script_path} 'devboot_image=devboot-image.bin'")
    barebox.run_check(f"echo -a {script_path} 'devboot_initrd=devboot-initrd.bin'")
    barebox.run_check(f"echo -a {script_path} 'devboot_oftree=/mnt/tftp/devboot-oftree.dtb'")
    barebox.run_check(f"echo -a {script_path} 'global devboot.script={arch}'")

    # Create a boot script that uses default files
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/devboottest '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/devboottest 'global.bootm.image={image_path}'")
    barebox.run_check(f"echo -a /env/boot/devboottest 'global.bootm.initrd={initrd_path}'")

    # Run devboot command
    barebox.run_check("devboot devboottest")

    assert barebox.run_check("echo $global.devboot.script") == [arch]

    # Verify devboot overrides were used
    assert hashsum(barebox, "/mnt/9p/testfs/devboot-image.bin") == hashsum(barebox, "/tmp/lastboot/image"), \
        "devboot image override was not used"
    assert hashsum(barebox, "/mnt/9p/testfs/devboot-initrd.bin") == hashsum(barebox, "/tmp/lastboot/initrd"), \
        "devboot initrd override was not used"

    barebox.run_check("test -f /tmp/lastboot/oftree")
    actual_model = of_get_property(barebox, "/model", file="/tmp/lastboot/oftree")
    expected_model = "Devboot Script DT"

    assert actual_model == expected_model, \
        f"devboot oftree should still be used when not overridden on cmdline (expected model '{expected_model}', got '{actual_model}')"

    # Clean up
    barebox.run("rm /env/boot/devboottest")
    barebox.run(f"rm {script_path}")


def test_devboot_script_hostname(barebox, bootm_mock, boot_testdata, devboot_testdata):  # noqa: ARG001
    """Test devboot with hostname-specific script (takes precedence over arch)"""

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")
    barebox.run("rm /mnt/9p/testfs/*-devboot-*")

    # Get global variables
    [user] = barebox.run_check("echo $global.user")
    [hostname] = barebox.run_check("echo $global.hostname")
    [arch] = barebox.run_check("echo $global.arch")

    # Create both hostname and arch scripts
    # Hostname script should take precedence
    hostname_script = f"{user}-devboot-{hostname}"
    arch_script = f"{user}-devboot-{arch}"

    hostname_script_path = f"/mnt/9p/testfs/{hostname_script}"
    arch_script_path = f"/mnt/9p/testfs/{arch_script}"

    # Hostname script sets devboot overrides
    barebox.run_check(f"echo -o {hostname_script_path} '#!/bin/sh'")
    barebox.run_check(f"echo -a {hostname_script_path} 'devboot_initrd=devboot-initrd.bin'")
    barebox.run_check(f"echo -a {hostname_script_path} 'global devboot.script={hostname}'")

    # Arch script sets different override (should not be used)
    barebox.run_check(f"echo -o {arch_script_path} '#!/bin/sh'")
    barebox.run_check(f"echo -a {arch_script_path} 'devboot_initrd={initrd_path}'")
    barebox.run_check(f"echo -a {arch_script_path} 'global devboot.script={arch}'")

    # Create a boot script
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/devboottest2 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/devboottest2 'global.bootm.image={image_path}'")

    # Run devboot command
    barebox.run_check("devboot devboottest2")
    assert barebox.run_check("echo $global.devboot.script") == [hostname]

    # Verify hostname script was used (devboot initrd)
    assert hashsum(barebox, "/mnt/9p/testfs/devboot-initrd.bin") == hashsum(barebox, "/tmp/lastboot/initrd"), \
        "Hostname-specific devboot script was not used"

    # Clean up
    barebox.run("rm /env/boot/devboottest2")
    barebox.run(f"rm {hostname_script_path}")
    barebox.run(f"rm {arch_script_path}")


def test_devboot_cmdline_override_precedence(barebox, bootm_mock, boot_testdata, devboot_testdata):  # noqa: ARG001
    """Test that command-line -o overrides take precedence over devboot script"""

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")
    barebox.run("rm /mnt/9p/testfs/${global.user}-devboot-*")

    # Get global variables
    [user] = barebox.run_check("echo $global.user")
    [arch] = barebox.run_check("echo $global.arch")

    # Create devboot script that sets initrd
    script_path = f"/mnt/9p/testfs/{user}-devboot-{arch}"

    barebox.run_check(f"echo -o {script_path} '#!/bin/sh'")
    barebox.run_check(f"echo -a {script_path} 'devboot_initrd=/mnt/9p/testfs/devboot-initrd.bin'")
    barebox.run_check(f"echo -a {script_path} 'devboot_oftree=/mnt/9p/testfs/devboot-oftree.dtb'")
    barebox.run_check(f"echo -a {script_path} 'global devboot.script={arch}'")

    # Create a boot script
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/devboottest3 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/devboottest3 'global.bootm.image={image_path}'")

    # Run devboot with cmdline override for initrd (should take precedence over script)
    barebox.run_check(f"devboot -o bootm.initrd={initrd_path} devboottest3")

    assert barebox.run_check("echo $global.devboot.script") == [arch]

    # Verify cmdline initrd was used (not devboot script's initrd)
    assert hashsum(barebox, initrd_path) == hashsum(barebox, "/tmp/lastboot/initrd"), \
        "Command-line override did not take precedence over devboot script"

    # Verify devboot oftree was still used (no cmdline override for it)
    # Check the model property instead of hash since bootm applies fixups
    barebox.run_check("test -f /tmp/lastboot/oftree")
    actual_model = of_get_property(barebox, "/model", file="/tmp/lastboot/oftree")
    expected_model = "Devboot Script DT"

    assert actual_model == expected_model, \
        f"devboot oftree should still be used when not overridden on cmdline (expected model '{expected_model}', got '{actual_model}')"

    # Clean up
    barebox.run("rm /env/boot/devboottest3")
    barebox.run(f"rm {script_path}")


# FIT image override type checking tests


def test_boot_override_fit_with_normal(barebox, barebox_config, bootm_mock, boot_testdata, make_fit_image, testfs):  # noqa: ARG001
    """Test that overriding a FIT image with a normal kernel succeeds (replaces FIT entirely)"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create a FIT image as the original
    outdir = Path(testfs)
    fit_image_data = b"#!/bin/sh\n# FIT KERNEL" + bytes(range(256)) * 16
    make_fit_image(outdir, fit_image_data, "test-fit-original.img")
    fit_image_path = "/mnt/9p/testfs/test-fit-original.img"

    # Create a normal kernel image as override
    normal_image_data = b"#!/bin/sh\n# NORMAL KERNEL" + bytes(range(256)) * 16
    (outdir / "test-normal-override.bin").write_bytes(normal_image_data)
    normal_image_path = "/mnt/9p/testfs/test-normal-override.bin"

    # Create a boot script that uses the FIT image
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testfit1 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/testfit1 'global.bootm.image={fit_image_path}'")

    # Override with normal kernel - should succeed (replaces FIT)
    barebox.run(f"boot -o bootm.image={normal_image_path} testfit1")

    # Verify the override image was used
    barebox.run_check("test -f /tmp/lastboot/image")
    assert hashsum(barebox, normal_image_path) == hashsum(barebox, "/tmp/lastboot/image"), \
        "Override normal kernel was not used to replace FIT image"

    # Clean up
    barebox.run("rm /env/boot/testfit1")
    barebox.run(f"rm {fit_image_path}")
    barebox.run(f"rm {normal_image_path}")


@pytest.mark.xfail(reason="Override normal kernel with FIT not supported")
def test_boot_override_normal_with_fit(barebox, barebox_config, bootm_mock, boot_testdata, make_fit_image, testfs):  # noqa: ARG001
    """Test that overriding a normal kernel with a FIT image fails due to type mismatch"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create a normal kernel image as the original
    outdir = Path(testfs)
    normal_image_data = b"#!/bin/sh\n# NORMAL KERNEL" + bytes(range(256)) * 16
    (outdir / "test-normal-original.bin").write_bytes(normal_image_data)
    normal_image_path = "/mnt/9p/testfs/test-normal-original.bin"

    # Create a FIT image as override
    fit_image_data = b"#!/bin/sh\n# FIT KERNEL" + bytes(range(256)) * 16
    make_fit_image(outdir, fit_image_data, "test-fit-override.img")
    fit_image_path = "/mnt/9p/testfs/test-fit-override.img"

    # Create a boot script that uses the normal image
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testfit2 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/testfit2 'global.bootm.image={normal_image_path}'")

    # Try to override with FIT image - should fail
    stdout, _, ret = barebox.run(f"boot -o bootm.image={fit_image_path} testfit2")
    assert ret != 0, "Override should have failed due to type mismatch"
    assert b"file type mismatch" in b"".join(stdout), "Error message should mention type mismatch"

    # Clean up
    barebox.run("rm /env/boot/testfit2")
    barebox.run(f"rm {normal_image_path}")
    barebox.run(f"rm {fit_image_path}")


def test_boot_override_fit_with_fit(barebox, barebox_config, bootm_mock, boot_testdata, make_fit_image, testfs):  # noqa: ARG001
    """Test that overriding a FIT image with another FIT image succeeds"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create a FIT image as the original
    outdir = Path(testfs)
    fit1_image_data = b"#!/bin/sh\n# FIT KERNEL 1" + bytes(range(256)) * 16
    make_fit_image(outdir, fit1_image_data, "test-fit-original2.img")
    fit1_image_path = "/mnt/9p/testfs/test-fit-original2.img"

    # Create another FIT image as override
    fit2_image_data = b"#!/bin/sh\n# FIT KERNEL 2" + bytes(range(256)) * 16
    make_fit_image(outdir, fit2_image_data, "test-fit-override2.img")
    fit2_image_path = "/mnt/9p/testfs/test-fit-override2.img"

    # Create a boot script that uses the first FIT image
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testfit3 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/testfit3 'global.bootm.image={fit1_image_path}'")

    # Override with second FIT image - should succeed
    barebox.run(f"boot -o bootm.image={fit2_image_path} testfit3")

    # Verify the override image was used
    # For FIT images, /tmp/lastboot/image contains the extracted kernel, not the FIT file
    # So we need to compare against the kernel data that was embedded in the FIT
    barebox.run_check("test -f /tmp/lastboot/image")
    fit2_kernel_path = "/mnt/9p/testfs/test-fit2-kernel.bin"
    (outdir / "test-fit2-kernel.bin").write_bytes(fit2_image_data)
    assert hashsum(barebox, fit2_kernel_path) == hashsum(barebox, "/tmp/lastboot/image"), \
        "Override FIT image kernel was not used"
    barebox.run(f"rm {fit2_kernel_path}")

    # Clean up
    barebox.run("rm /env/boot/testfit3")
    barebox.run(f"rm {fit1_image_path}")
    barebox.run(f"rm {fit2_image_path}")


def test_boot_override_fit_with_fit_and_initrd(barebox, barebox_config, bootm_mock, boot_testdata, make_fit_image, testfs):  # noqa: ARG001
    """Test overriding a FIT image (with embedded initrd) with another FIT and separate initrd"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create a FIT image with embedded initrd as the original
    outdir = Path(testfs)
    fit1_image_data = b"#!/bin/sh\n# FIT KERNEL 1" + bytes(range(256)) * 16
    fit1_initrd_data = b"#!/bin/sh\n# FIT INITRD 1" + bytes(range(256)) * 16
    make_fit_image(outdir, fit1_image_data, "test-fit-with-initrd1.img", initrd_data=fit1_initrd_data)
    fit1_image_path = "/mnt/9p/testfs/test-fit-with-initrd1.img"

    # Create another FIT image as override
    fit2_image_data = b"#!/bin/sh\n# FIT KERNEL 2" + bytes(range(256)) * 16
    make_fit_image(outdir, fit2_image_data, "test-fit-override3.img")
    fit2_image_path = "/mnt/9p/testfs/test-fit-override3.img"

    # Create a separate initrd for override
    separate_initrd_data = b"#!/bin/sh\n# SEPARATE INITRD" + bytes(range(256)) * 16
    (outdir / "test-separate-initrd.bin").write_bytes(separate_initrd_data)
    separate_initrd_path = "/mnt/9p/testfs/test-separate-initrd.bin"

    # Create a boot script that uses the first FIT image
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testfit4 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/testfit4 'global.bootm.image={fit1_image_path}'")

    # Override with second FIT image and separate initrd - should succeed
    barebox.run(f"boot -o bootm.image={fit2_image_path} -o bootm.initrd={separate_initrd_path} testfit4")

    # Verify the override image and initrd were used
    # For FIT images, /tmp/lastboot/image contains the extracted kernel, not the FIT file
    barebox.run_check("test -f /tmp/lastboot/image")
    barebox.run_check("test -f /tmp/lastboot/initrd")
    fit2_kernel_path = "/mnt/9p/testfs/test-fit2-kernel2.bin"
    (outdir / "test-fit2-kernel2.bin").write_bytes(fit2_image_data)
    assert hashsum(barebox, fit2_kernel_path) == hashsum(barebox, "/tmp/lastboot/image"), \
        "Override FIT image kernel was not used"
    barebox.run(f"rm {fit2_kernel_path}")
    assert hashsum(barebox, separate_initrd_path) == hashsum(barebox, "/tmp/lastboot/initrd"), \
        "Separate initrd override was not used"

    # Clean up
    barebox.run("rm /env/boot/testfit4")
    barebox.run(f"rm {fit1_image_path}")
    barebox.run(f"rm {fit2_image_path}")
    barebox.run(f"rm {separate_initrd_path}")


@pytest.mark.xfail(reason="Override normal kernel with FIT not supported")
def test_boot_override_normal_with_fit_and_initrd(barebox, barebox_config, bootm_mock, boot_testdata, make_fit_image, testfs):  # noqa: ARG001
    """Test that overriding a normal kernel with FIT and separate initrd fails for image (type mismatch)"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")

    # Create a normal kernel image as the original
    outdir = Path(testfs)
    normal_image_data = b"#!/bin/sh\n# NORMAL KERNEL" + bytes(range(256)) * 16
    (outdir / "test-normal-original2.bin").write_bytes(normal_image_data)
    normal_image_path = "/mnt/9p/testfs/test-normal-original2.bin"

    # Create a FIT image as override
    fit_image_data = b"#!/bin/sh\n# FIT KERNEL" + bytes(range(256)) * 16
    make_fit_image(outdir, fit_image_data, "test-fit-override4.img")
    fit_image_path = "/mnt/9p/testfs/test-fit-override4.img"

    # Create a separate initrd for override
    separate_initrd_data = b"#!/bin/sh\n# SEPARATE INITRD" + bytes(range(256)) * 16
    (outdir / "test-separate-initrd2.bin").write_bytes(separate_initrd_data)
    separate_initrd_path = "/mnt/9p/testfs/test-separate-initrd2.bin"

    # Create a boot script that uses the normal image
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/testfit5 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/testfit5 'global.bootm.image={normal_image_path}'")

    # Try to override with FIT image and separate initrd - should fail due to image type mismatch
    stdout, _, ret = barebox.run(f"boot -o bootm.image={fit_image_path} -o bootm.initrd={separate_initrd_path} testfit5")
    assert ret != 0, "Override should have failed due to image type mismatch"
    assert b"file type mismatch" in b"".join(stdout), "Error message should mention type mismatch"

    # Clean up
    barebox.run("rm /env/boot/testfit5")
    barebox.run(f"rm {normal_image_path}")
    barebox.run(f"rm {fit_image_path}")
    barebox.run(f"rm {separate_initrd_path}")


def test_devboot_fit_override_with_empty_initrd(barebox, barebox_config, bootm_mock, boot_testdata, make_fit_image, testfs, compile_dtb):  # noqa: ARG001
    """Test devboot overriding FIT image with another FIT and empty initrd (no initrd used)"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")
    barebox.run("rm /mnt/9p/testfs/*-devboot-*")

    outdir = Path(testfs)

    # Create original FIT image with kernel, initrd, and DT
    fit1_kernel_data = b"#!/bin/sh\n# ORIGINAL FIT KERNEL" + bytes(range(256)) * 16
    fit1_initrd_data = b"#!/bin/sh\n# ORIGINAL FIT INITRD" + bytes(range(256)) * 16
    fit1_dtb_dts = """/dts-v1/;
/ {
    model = "Original FIT DT";
};
"""
    compile_dtb(testfs, fit1_dtb_dts, "fit1.dtb")
    fit1_dtb_data = (outdir / "fit1.dtb").read_bytes()
    make_fit_image(outdir, fit1_kernel_data, "original-fit.img", initrd_data=fit1_initrd_data, dtb_data=fit1_dtb_data)
    fit1_path = "/mnt/9p/testfs/original-fit.img"

    # Create override FIT image with different kernel, initrd, and DT
    fit2_kernel_data = b"#!/bin/sh\n# OVERRIDE FIT KERNEL" + bytes(range(256)) * 16
    fit2_initrd_data = b"#!/bin/sh\n# OVERRIDE FIT INITRD" + bytes(range(256)) * 16
    fit2_dtb_dts = """/dts-v1/;
/ {
    model = "Override FIT DT";
};
"""
    compile_dtb(testfs, fit2_dtb_dts, "fit2.dtb")
    fit2_dtb_data = (outdir / "fit2.dtb").read_bytes()
    make_fit_image(outdir, fit2_kernel_data, "override-fit.img", initrd_data=fit2_initrd_data, dtb_data=fit2_dtb_data)
    fit2_path = "/mnt/9p/testfs/override-fit.img"

    # Get global variables for devboot script
    [user] = barebox.run_check("echo $global.user")
    [arch] = barebox.run_check("echo $global.arch")
    script_path = f"/mnt/9p/testfs/{user}-devboot-{arch}"

    # Create devboot script that overrides image and sets initrd to empty
    barebox.run_check(f"echo -o {script_path} '#!/bin/sh'")
    barebox.run_check(f"echo -a {script_path} 'devboot_image={fit2_path}'")
    barebox.run_check(f"echo -a {script_path} 'devboot_initrd='")  # Empty initrd
    barebox.run_check(f"echo -a {script_path} 'global devboot.script={arch}'")

    # Create a boot script that uses the original FIT
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/devboot_fit_test1 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/devboot_fit_test1 'global.bootm.image={fit1_path}'")

    # Run devboot
    barebox.run_check("devboot devboot_fit_test1")

    assert barebox.run_check("echo $global.devboot.script") == [arch]

    # Verify override FIT kernel was used
    barebox.run_check("test -f /tmp/lastboot/image")
    fit2_kernel_path = "/mnt/9p/testfs/fit2-kernel.bin"
    (outdir / "fit2-kernel.bin").write_bytes(fit2_kernel_data)
    assert hashsum(barebox, fit2_kernel_path) == hashsum(barebox, "/tmp/lastboot/image"), \
        "Override FIT kernel was not used"

    # Verify NO initrd was used (file should not exist or be empty)
    _, _, ret = barebox.run("test -f /tmp/lastboot/initrd")
    assert ret != 0, "Initrd file should not exist when devboot_initrd is empty"

    # Clean up
    barebox.run("rm /env/boot/devboot_fit_test1")
    barebox.run(f"rm {script_path}")
    barebox.run(f"rm {fit1_path} {fit2_path} {fit2_kernel_path}")
    barebox.run("rm /mnt/9p/testfs/fit1.dtb /mnt/9p/testfs/fit2.dtb")


def test_devboot_fit_override_with_two_initrds(barebox, barebox_config, bootm_mock, boot_testdata, make_fit_image, testfs, compile_dtb):  # noqa: ARG001
    """Test devboot overriding FIT image with another FIT and two separate initrd files (concatenated)"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")
    barebox.run("rm /mnt/9p/testfs/*-devboot-*")

    outdir = Path(testfs)

    # Create original FIT image with kernel, initrd, and DT
    fit1_kernel_data = b"#!/bin/sh\n# ORIGINAL FIT KERNEL 2" + bytes(range(256)) * 16
    fit1_initrd_data = b"#!/bin/sh\n# ORIGINAL FIT INITRD 2" + bytes(range(256)) * 16
    fit1_dtb_dts = """/dts-v1/;
/ {
    model = "Original FIT DT 2";
};
"""
    compile_dtb(testfs, fit1_dtb_dts, "fit1-test2.dtb")
    fit1_dtb_data = (outdir / "fit1-test2.dtb").read_bytes()
    make_fit_image(outdir, fit1_kernel_data, "original-fit2.img", initrd_data=fit1_initrd_data, dtb_data=fit1_dtb_data)
    fit1_path = "/mnt/9p/testfs/original-fit2.img"

    # Create override FIT image with different kernel, initrd, and DT
    fit2_kernel_data = b"#!/bin/sh\n# OVERRIDE FIT KERNEL 2" + bytes(range(256)) * 16
    fit2_initrd_data = b"#!/bin/sh\n# OVERRIDE FIT INITRD 2" + bytes(range(256)) * 16
    fit2_dtb_dts = """/dts-v1/;
/ {
    model = "Override FIT DT 2";
};
"""
    compile_dtb(testfs, fit2_dtb_dts, "fit2-test2.dtb")
    fit2_dtb_data = (outdir / "fit2-test2.dtb").read_bytes()
    make_fit_image(outdir, fit2_kernel_data, "override-fit2.img", initrd_data=fit2_initrd_data, dtb_data=fit2_dtb_data)
    fit2_path = "/mnt/9p/testfs/override-fit2.img"

    # Create two separate initrd files
    initrd1_data = b"#!/bin/sh\n# SEPARATE INITRD 1" + bytes(range(256)) * 8
    initrd2_data = b"#!/bin/sh\n# SEPARATE INITRD 2" + bytes(range(256)) * 8
    (outdir / "separate-initrd1.bin").write_bytes(initrd1_data)
    (outdir / "separate-initrd2.bin").write_bytes(initrd2_data)
    initrd1_path = "/mnt/9p/testfs/separate-initrd1.bin"
    initrd2_path = "/mnt/9p/testfs/separate-initrd2.bin"

    # Get global variables for devboot script
    [user] = barebox.run_check("echo $global.user")
    [arch] = barebox.run_check("echo $global.arch")
    script_path = f"/mnt/9p/testfs/{user}-devboot-{arch}"

    barebox.run_check(f"global devboot.script=")

    # Create devboot script that overrides image and sets initrd to two files
    with open(Path(testfs) / f"{user}-devboot-{arch}", 'w', encoding='utf-8') as outf:
            outf.write(f"""
                #!/bin/sh
                devboot_image={fit2_path}
                devboot_initrd="{initrd1_path} {initrd2_path}"
                global devboot.script={arch}-2initrd
            """)

    # Create a boot script that uses the original FIT
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/devboot_fit_test2 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/devboot_fit_test2 'global.bootm.image={fit1_path}'")

    # Run devboot
    barebox.run_check("devboot devboot_fit_test2")

    assert barebox.run_check("echo $global.devboot.script") == [f"{arch}-2initrd"]

    # Verify override FIT kernel was used
    barebox.run_check("test -f /tmp/lastboot/image")
    fit2_kernel_path = "/mnt/9p/testfs/fit2-kernel2.bin"
    (outdir / "fit2-kernel2.bin").write_bytes(fit2_kernel_data)
    assert hashsum(barebox, fit2_kernel_path) == hashsum(barebox, "/tmp/lastboot/image"), \
        "Override FIT kernel was not used"

    # Verify the two initrds were concatenated (not the FIT's initrd)
    barebox.run_check("test -f /tmp/lastboot/initrd")
    concatenated_initrd_data = initrd1_data + initrd2_data
    concat_initrd_path = "/mnt/9p/testfs/concat-initrd.bin"
    (outdir / "concat-initrd.bin").write_bytes(concatenated_initrd_data)
    assert hashsum(barebox, concat_initrd_path) == hashsum(barebox, "/tmp/lastboot/initrd"), \
        "Concatenated initrds were not used"

    # Clean up
    barebox.run("rm /env/boot/devboot_fit_test2")
    barebox.run(f"rm {fit1_path} {fit2_path} {fit2_kernel_path}")
    barebox.run(f"rm {initrd1_path} {initrd2_path} {concat_initrd_path}")
    barebox.run("rm /mnt/9p/testfs/fit1-test2.dtb /mnt/9p/testfs/fit2-test2.dtb")


def test_devboot_fit_override_with_at_and_two_initrds(barebox, barebox_config, bootm_mock, boot_testdata, make_fit_image, testfs, compile_dtb):  # noqa: ARG001
    """Test devboot overriding FIT image with another FIT and @ plus two initrd files (all three concatenated)"""

    skip_disabled(barebox_config, "CONFIG_BOOT_OVERRIDE")

    # Clean up any previous test data
    barebox.run("rm -rf /tmp/lastboot")
    barebox.run("rm /mnt/9p/testfs/*-devboot-*")

    outdir = Path(testfs)

    # Create original FIT image with kernel, initrd, and DT
    fit1_kernel_data = b"#!/bin/sh\n# ORIGINAL FIT KERNEL 3" + bytes(range(256)) * 16
    fit1_initrd_data = b"#!/bin/sh\n# ORIGINAL FIT INITRD 3" + bytes(range(256)) * 16
    fit1_dtb_dts = """/dts-v1/;
/ {
    model = "Original FIT DT 3";
};
"""
    compile_dtb(testfs, fit1_dtb_dts, "fit1-test3.dtb")
    fit1_dtb_data = (outdir / "fit1-test3.dtb").read_bytes()
    make_fit_image(outdir, fit1_kernel_data, "original-fit3.img", initrd_data=fit1_initrd_data, dtb_data=fit1_dtb_data)
    fit1_path = "/mnt/9p/testfs/original-fit3.img"

    # Create override FIT image with different kernel, initrd, and DT
    fit2_kernel_data = b"#!/bin/sh\n# OVERRIDE FIT KERNEL 3" + bytes(range(256)) * 16
    fit2_initrd_data = b"#!/bin/sh\n# OVERRIDE FIT INITRD 3" + bytes(range(256)) * 16
    fit2_dtb_dts = """/dts-v1/;
/ {
    model = "Override FIT DT 3";
};
"""
    compile_dtb(testfs, fit2_dtb_dts, "fit2-test3.dtb")
    fit2_dtb_data = (outdir / "fit2-test3.dtb").read_bytes()
    make_fit_image(outdir, fit2_kernel_data, "override-fit3.img", initrd_data=fit2_initrd_data, dtb_data=fit2_dtb_data)
    fit2_path = "/mnt/9p/testfs/override-fit3.img"

    # Create two separate initrd files
    initrd1_data = b"#!/bin/sh\n# SEPARATE INITRD 3A" + bytes(range(256)) * 8
    initrd2_data = b"#!/bin/sh\n# SEPARATE INITRD 3B" + bytes(range(256)) * 8
    (outdir / "separate-initrd3a.bin").write_bytes(initrd1_data)
    (outdir / "separate-initrd3b.bin").write_bytes(initrd2_data)
    initrd1_path = "/mnt/9p/testfs/separate-initrd3a.bin"
    initrd2_path = "/mnt/9p/testfs/separate-initrd3b.bin"

    # Get global variables for devboot script
    [user] = barebox.run_check("echo $global.user")
    [arch] = barebox.run_check("echo $global.arch")
    script_path = f"/mnt/9p/testfs/{user}-devboot-{arch}"

    # Create devboot script that overrides image and sets initrd to @ plus two files
    # @ means the override FIT's initrd should be included
    with open(Path(testfs) / f"{user}-devboot-{arch}", 'w', encoding='utf-8') as outf:
            outf.write(f"""
                #!/bin/sh
                devboot_image={fit2_path}
                devboot_initrd="@ {initrd1_path} {initrd2_path}"
                global devboot.script={arch}-2-at-initrd
            """)

    # Create a boot script that uses the original FIT
    barebox.run_check("mkdir -p /env/boot")
    barebox.run_check("echo -o /env/boot/devboot_fit_test3 '#!/bin/sh'")
    barebox.run_check(f"echo -a /env/boot/devboot_fit_test3 'global.bootm.image={fit1_path}'")

    # Run devboot
    barebox.run_check("devboot devboot_fit_test3")

    assert barebox.run_check("echo $global.devboot.script") == [f"{arch}-2-at-initrd"]

    # Verify override FIT kernel was used
    barebox.run_check("test -f /tmp/lastboot/image")
    fit2_kernel_path = "/mnt/9p/testfs/fit2-kernel3.bin"
    (outdir / "fit2-kernel3.bin").write_bytes(fit2_kernel_data)
    assert hashsum(barebox, fit2_kernel_path) == hashsum(barebox, "/tmp/lastboot/image"), \
        "Override FIT kernel was not used"

    # Verify all three initrds were concatenated (FIT2's initrd + two separate files)
    barebox.run_check("test -f /tmp/lastboot/initrd")
    concatenated_initrd_data = fit2_initrd_data + initrd1_data + initrd2_data
    concat_initrd_path = "/mnt/9p/testfs/concat-initrd3.bin"
    (outdir / "concat-initrd3.bin").write_bytes(concatenated_initrd_data)
    assert hashsum(barebox, concat_initrd_path) == hashsum(barebox, "/tmp/lastboot/initrd"), \
        "All three initrds (override FIT + two separate) were not concatenated correctly"

    # Clean up
    barebox.run("rm /env/boot/devboot_fit_test3")
    barebox.run(f"rm {script_path}")
    barebox.run(f"rm {fit1_path} {fit2_path} {fit2_kernel_path}")
    barebox.run(f"rm {initrd1_path} {initrd2_path} {concat_initrd_path}")
    barebox.run("rm /mnt/9p/testfs/fit1-test3.dtb /mnt/9p/testfs/fit2-test3.dtb")
