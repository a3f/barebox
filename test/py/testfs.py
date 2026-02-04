# SPDX-License-Identifier: GPL-2.0-or-later

import subprocess


def mkcpio(inpath, outname):
    compress = outname.suffix == ".gz"

    find = subprocess.Popen(["find", inpath], stdout=subprocess.PIPE)

    with open(outname, "wb") as outfile:
        cpio = subprocess.Popen(["cpio", "-o", "-H", "newc"], stdin=find.stdout,
                                stdout=(subprocess.PIPE if compress else outfile))

        find.stdout.close()

        if compress:
            gzip = subprocess.Popen(["gzip"], stdin=cpio.stdout, stdout=outfile)
            cpio.stdout.close()
            gzip.wait()

        cpio.wait()
        find.wait()


# Compression algorithms with their barebox config, compress command, and file extension
# Settings aligned with Linux kernel's scripts/Makefile.lib
COMPRESSION_ALGOS = [
    ("gzip", "CONFIG_ZLIB", ["gzip", "-n", "-f", "-9"], ".gz"),
    ("bzip2", "CONFIG_BZLIB", ["bzip2", "-9"], ".bz2"),
    ("lz4", "CONFIG_LZ4_DECOMPRESS", ["lz4", "-l", "-9"], ".lz4"),
    # xz: use xzmisc settings (--check=crc32 --lzma2=dict=1MiB)
    ("xz", "CONFIG_XZ_DECOMPRESS", ["xz", "--check=crc32", "--lzma2=dict=1MiB"], ".xz"),
    ("zstd", "CONFIG_ZSTD_DECOMPRESS", ["zstd", "-19"], ".zst"),
]


def compress_file(infile, outfile, algo_name):
    """Compress a file using the specified algorithm.

    Args:
        infile: Path to the input file to compress
        outfile: Path to write the compressed output
        algo_name: Name of the compression algorithm (gzip, bzip2, lz4, xz, zstd)

    Raises:
        ValueError: If algo_name is not a known algorithm
        subprocess.CalledProcessError: If compression fails
    """
    for name, _, cmd, _ in COMPRESSION_ALGOS:
        if name == algo_name:
            with open(outfile, "wb") as out:
                subprocess.run(cmd, input=infile.read_bytes(), stdout=out, check=True)
            return
    raise ValueError(f"Unknown algorithm: {algo_name}")
