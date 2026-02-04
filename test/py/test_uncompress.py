# SPDX-License-Identifier: GPL-2.0-or-later

import json
import os
import pytest
import shutil
import subprocess
from pathlib import Path
from .helper import skip_disabled
from .testfs import COMPRESSION_ALGOS, compress_file

# Cache for memory gap to avoid repeated iomem calls
_memory_gap_cache = None


@pytest.fixture
def memory_gap(barebox, barebox_config):
    """Find a suitable memory gap for testing using iomem -jg.

    Returns a tuple (start_addr, size) of the first gap large enough for testing.
    Looks for free gaps within RAM regions (ram0, sdram, etc.), not just any gap.
    Result is cached to avoid repeated iomem calls.
    """
    global _memory_gap_cache
    if _memory_gap_cache is not None:
        return _memory_gap_cache

    skip_disabled(barebox_config, "CONFIG_CMD_IOMEM")

    stdout = barebox.run_check("iomem -jg")
    iomem = json.loads("\n".join(stdout))

    def find_ram_gaps(region, gaps, in_ram=False):
        """Recursively find free gaps within RAM regions."""
        name = region.get("name", "").lower()
        # Check if this is a RAM region
        is_ram = in_ram or "ram" in name or "sdram" in name

        if is_ram and region.get("free"):
            start = int(region["start"], 16)
            end = int(region["end"], 16)
            size = end - start + 1
            gaps.append((start, size))

        for child in region.get("children", []):
            find_ram_gaps(child, gaps, is_ram)
        return gaps

    gaps = find_ram_gaps(iomem, [])

    # Need at least 1 MiB for decompression tests (compressed + decompressed + margin)
    min_size = 1024 * 1024
    for start, size in gaps:
        if size >= min_size:
            _memory_gap_cache = (start, size)
            return _memory_gap_cache

    pytest.skip(f"No memory gap >= {min_size} bytes found in RAM regions")


# Test data size - 64KB is enough to test decompression without being too slow
TEST_DATA_SIZE = 64 * 1024


@pytest.fixture(scope="module")
def uncompress_testdata(barebox_config, testfs):
    """Prepare compressed test files for all supported algorithms."""
    skip_disabled(barebox_config, "CONFIG_CMD_UNCOMPRESS")

    builddir = os.environ.get('LG_BUILDDIR')
    if not builddir:
        pytest.skip("LG_BUILDDIR not set")

    builddir = Path(builddir)
    outdir = Path(testfs)

    # Use first TEST_DATA_SIZE bytes of barebox-dt-2nd.img as test data
    barebox_img = builddir / "images" / "barebox-dt-2nd.img"
    if not barebox_img.exists():
        pytest.skip(f"barebox-dt-2nd.img not found at {barebox_img}")

    # Copy only first TEST_DATA_SIZE bytes to testfs for reference
    original = outdir / "test-original.bin"
    with open(barebox_img, "rb") as src, open(original, "wb") as dst:
        dst.write(src.read(TEST_DATA_SIZE))

    # Create compressed versions for each available algorithm
    compressed_files = {}
    for name, config, cmd, ext in COMPRESSION_ALGOS:
        if shutil.which(cmd[0]) is None:
            continue
        if config not in barebox_config:
            continue

        compressed = outdir / f"test-compressed{ext}"
        try:
            compress_file(original, compressed, name)
            compressed_files[name] = {
                "path": compressed,
                "ext": ext,
            }
        except subprocess.CalledProcessError:
            # Skip this algorithm if compression fails
            continue

    if not compressed_files:
        pytest.skip("No compression algorithms available")

    return {
        "original": original,
        "original_size": original.stat().st_size,
        "compressed": compressed_files,
    }


def algo_ids():
    """Generate test IDs for parametrization."""
    return [name for name, _, _, _ in COMPRESSION_ALGOS]


def first_available_algo(uncompress_testdata):
    """Return the first available compression algorithm from testdata."""
    return next(iter(uncompress_testdata["compressed"].keys()))


@pytest.fixture(params=algo_ids())
def algo(request, barebox_config, uncompress_testdata):
    """Parametrized fixture for each compression algorithm."""
    algo_name = request.param

    # Find the config for this algorithm
    for name, config, cmd, _ in COMPRESSION_ALGOS:
        if name == algo_name:
            if shutil.which(cmd[0]) is None:
                pytest.skip(f"{cmd[0]} not available on host")
            skip_disabled(barebox_config, config)
            if algo_name not in uncompress_testdata["compressed"]:
                pytest.skip(f"Compressed file for {algo_name} not available")
            return algo_name

    pytest.skip(f"Unknown algorithm: {algo_name}")


def get_original_md5(barebox, testfs_path):
    """Get MD5 hash of the original file."""
    [hashline] = barebox.run_check(f"md5sum {testfs_path}/test-original.bin")
    return hashline.split()[0]


def test_uncompress_file_to_file(barebox, uncompress_testdata, algo):
    """Test uncompressing from file to file (original behavior)."""
    testfs_path = "/mnt/9p/testfs"
    ext = uncompress_testdata["compressed"][algo]["ext"]

    # Get expected MD5 hash
    expected_md5 = get_original_md5(barebox, testfs_path)

    # Uncompress to file
    barebox.run_check(f"uncompress {testfs_path}/test-compressed{ext} /tmp/decompressed.bin")

    # Verify with MD5
    [hashline] = barebox.run_check("md5sum /tmp/decompressed.bin")
    actual_md5 = hashline.split()[0]

    assert expected_md5 == actual_md5, f"MD5 mismatch for {algo} file-to-file"

    # Cleanup
    barebox.run("rm /tmp/decompressed.bin")


def test_uncompress_file_to_memory(barebox, barebox_config, uncompress_testdata, algo, memory_gap):
    """Test uncompressing from file to memory region."""
    skip_disabled(barebox_config, "CONFIG_CMD_UNCOMPRESS_MEMORY")

    testfs_path = "/mnt/9p/testfs"
    ext = uncompress_testdata["compressed"][algo]["ext"]
    original_size = uncompress_testdata["original_size"]

    # Get expected MD5 hash
    expected_md5 = get_original_md5(barebox, testfs_path)

    # Use the memory gap found by fixture
    dest_addr, gap_size = memory_gap

    # Uncompress from file to memory with explicit size
    # Add plenty of extra space for safety (10x the original)
    dest_size = original_size * 10
    assert dest_size <= gap_size, f"Memory gap too small: {gap_size} < {dest_size}"

    barebox.run_check(
        f"uncompress {testfs_path}/test-compressed{ext} "
        f"0x{dest_addr:x}+0x{dest_size:x}"
    )

    # Verify with MD5 on memory region
    [hashline] = barebox.run_check(f"md5sum 0x{dest_addr:x}+0x{original_size:x}")
    actual_md5 = hashline.split()[0]

    assert expected_md5 == actual_md5, f"MD5 mismatch for {algo} file-to-memory"


def test_uncompress_memory_to_memory(barebox, barebox_config, uncompress_testdata, memory_gap):
    """Test uncompressing from memory to memory region."""
    skip_disabled(barebox_config, "CONFIG_CMD_UNCOMPRESS_MEMORY")

    testfs_path = "/mnt/9p/testfs"
    algo = first_available_algo(uncompress_testdata)
    ext = uncompress_testdata["compressed"][algo]["ext"]
    original_size = uncompress_testdata["original_size"]

    # Get expected MD5 hash
    expected_md5 = get_original_md5(barebox, testfs_path)

    # Get compressed file size
    [ls_output] = barebox.run_check(f"ls -l {testfs_path}/test-compressed{ext}")
    compressed_size = int(ls_output.split()[1])

    # Use the memory gap found by fixture
    base_addr, gap_size = memory_gap

    # Layout: [compressed data] [gap] [decompressed data]
    src_addr = base_addr
    dest_size = original_size * 10
    dest_addr = base_addr + compressed_size + 0x10000  # Leave gap after compressed data

    total_needed = compressed_size + 0x10000 + dest_size
    assert total_needed <= gap_size, f"Memory gap too small: {gap_size} < {total_needed}"

    # First copy compressed data to memory using memcpy
    # memcpy syntax: memcpy -s FILE SRC_OFFSET DEST COUNT
    barebox.run_check(
        f"memcpy -s {testfs_path}/test-compressed{ext} "
        f"0 0x{src_addr:x} 0x{compressed_size:x}"
    )

    # Uncompress from memory to memory
    barebox.run_check(
        f"uncompress 0x{src_addr:x}+0x{compressed_size:x} "
        f"0x{dest_addr:x}+0x{dest_size:x}"
    )

    # Verify with MD5 on destination memory region
    [hashline] = barebox.run_check(f"md5sum 0x{dest_addr:x}+0x{original_size:x}")
    actual_md5 = hashline.split()[0]

    assert expected_md5 == actual_md5, "MD5 mismatch for memory-to-memory"


def test_uncompress_memory_to_file(barebox, barebox_config, uncompress_testdata, memory_gap):
    """Test uncompressing from memory to file."""
    skip_disabled(barebox_config, "CONFIG_CMD_UNCOMPRESS_MEMORY")

    testfs_path = "/mnt/9p/testfs"
    algo = first_available_algo(uncompress_testdata)
    ext = uncompress_testdata["compressed"][algo]["ext"]

    # Get expected MD5 hash
    expected_md5 = get_original_md5(barebox, testfs_path)

    [ls_output] = barebox.run_check(f"ls -l {testfs_path}/test-compressed{ext}")
    compressed_size = int(ls_output.split()[1])

    # Use the memory gap found by fixture
    src_addr, _ = memory_gap

    # Copy compressed data to memory
    barebox.run_check(
        f"memcpy -s {testfs_path}/test-compressed{ext} "
        f"0 0x{src_addr:x} 0x{compressed_size:x}"
    )

    # Uncompress from memory to file
    barebox.run_check(
        f"uncompress 0x{src_addr:x}+0x{compressed_size:x} /tmp/decompressed.bin"
    )

    # Verify with MD5
    [hashline] = barebox.run_check("md5sum /tmp/decompressed.bin")
    actual_md5 = hashline.split()[0]

    assert expected_md5 == actual_md5, "MD5 mismatch for memory-to-file"

    # Cleanup
    barebox.run("rm /tmp/decompressed.bin")


def test_uncompress_missing_dest_size_error(barebox, barebox_config, uncompress_testdata, memory_gap):
    """Test that memory destination without size gives an error."""
    skip_disabled(barebox_config, "CONFIG_CMD_UNCOMPRESS_MEMORY")

    testfs_path = "/mnt/9p/testfs"
    algo = first_available_algo(uncompress_testdata)
    ext = uncompress_testdata["compressed"][algo]["ext"]
    base_addr, _ = memory_gap

    # This should fail - no size specified for memory destination
    _, _, ret = barebox.run(
        f"uncompress {testfs_path}/test-compressed{ext} 0x{base_addr:x}"
    )
    assert ret != 0, "Expected error for memory destination without size"
