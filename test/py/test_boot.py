# SPDX-License-Identifier: GPL-2.0-or-later

import pytest
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
