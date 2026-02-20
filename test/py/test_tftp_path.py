# SPDX-License-Identifier: GPL-2.0-or-later
#
# Tests for TFTP filesystem path resolution, specifically that relative
# paths containing subdirectories work when CWD is on a TFTP mount.

import pytest
from pathlib import Path
from .helper import skip_disabled


tftpdir = "/mnt/tftp0"


@pytest.fixture(scope="function")
def tftp_mount(barebox, barebox_config, env, testfs):
    """Bring up networking and mount TFTP filesystem."""
    skip_disabled(barebox_config, "CONFIG_FS_TFTP")

    if 'network' not in env.get_target_features():
        pytest.skip("network feature not available")

    barebox.run("ifup -a")
    barebox.run_check(f"mkdir -p {tftpdir}")
    # Mount if not already mounted; ignore "Device or resource busy"
    barebox.run(f"mount -t tftp $eth0.serverip {tftpdir}")

    yield tftpdir

    barebox.run("cd /")


def test_tftp_relative_flat(barebox, testfs, tftp_mount):
    """Test relative path to a flat file from CWD on TFTP mount."""
    outdir = Path(testfs)
    (outdir / "relflat.txt").write_text("flat\n")

    barebox.run_check(f"cd {tftpdir}")
    [line] = barebox.run_check("cat relflat.txt")
    barebox.run_check("cd /")
    assert line == "flat"

    (outdir / "relflat.txt").unlink()


def test_tftp_absolute_subdir(barebox, testfs, tftp_mount):
    """Test absolute path with subdirectory on TFTP mount.

    This is handled by the in-loop dentry_is_tftp() check and should
    work regardless of the pre-loop fix.
    """
    outdir = Path(testfs)
    (outdir / "tftpsub").mkdir(exist_ok=True)
    (outdir / "tftpsub" / "absfile.txt").write_text("abs-sub\n")

    [line] = barebox.run_check(f"cat {tftpdir}/tftpsub/absfile.txt")
    assert line == "abs-sub"

    (outdir / "tftpsub" / "absfile.txt").unlink()
    (outdir / "tftpsub").rmdir()


def test_tftp_relative_subdir(barebox, testfs, tftp_mount):
    """Test relative path with subdirectory from CWD on TFTP mount.

    This is the key regression test: without the pre-loop dentry_is_tftp()
    check, 'subdir/file' from a TFTP CWD fails because the first component
    is looked up as a directory, which TFTP does not support.
    """
    outdir = Path(testfs)
    (outdir / "tftpsub").mkdir(exist_ok=True)
    (outdir / "tftpsub" / "relfile.txt").write_text("rel-sub\n")

    barebox.run_check(f"cd {tftpdir}")
    [line] = barebox.run_check("cat tftpsub/relfile.txt")
    barebox.run_check("cd /")
    assert line == "rel-sub"

    (outdir / "tftpsub" / "relfile.txt").unlink()
    (outdir / "tftpsub").rmdir()


def test_tftp_relative_dot_slash_subdir(barebox, testfs, tftp_mount):
    """Test ./-prefixed relative path with subdirectory on TFTP mount.

    This worked even before the fix because '.' resolves via handle_dots(),
    landing on the TFTP dentry which triggers the in-loop separator change.
    """
    outdir = Path(testfs)
    (outdir / "tftpsub").mkdir(exist_ok=True)
    (outdir / "tftpsub" / "dotfile.txt").write_text("dot-sub\n")

    barebox.run_check(f"cd {tftpdir}")
    [line] = barebox.run_check("cat ./tftpsub/dotfile.txt")
    barebox.run_check("cd /")
    assert line == "dot-sub"

    (outdir / "tftpsub" / "dotfile.txt").unlink()
    (outdir / "tftpsub").rmdir()


def test_tftp_relative_nested_subdir(barebox, testfs, tftp_mount):
    """Test relative path with nested subdirectories from CWD on TFTP mount.

    Verifies that 'a/b/file' is fully collapsed into a single TFTP filename
    when CWD is on a TFTP mount.  Without the pre-loop separator change the
    first component 'a' would be looked up as a directory and fail.
    """
    outdir = Path(testfs)
    (outdir / "a").mkdir(exist_ok=True)
    (outdir / "a" / "b").mkdir(exist_ok=True)
    (outdir / "a" / "b" / "deep.txt").write_text("nested\n")

    barebox.run_check(f"cd {tftpdir}")
    [line] = barebox.run_check("cat a/b/deep.txt")
    barebox.run_check("cd /")
    assert line == "nested"

    (outdir / "a" / "b" / "deep.txt").unlink()
    (outdir / "a" / "b").rmdir()
    (outdir / "a").rmdir()
