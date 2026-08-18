#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Build the barebox fuzz targets for OSS-Fuzz. This script is executed
# by build.sh in the OSS-Fuzz projects/barebox directory, but defaults
# all OSS-Fuzz environment variables so it can be run directly on a
# developer machine as well:
#
#   ./scripts/oss-fuzz.sh
#   ./oss-fuzz-work/out/fuzz-filetype
#
# Relevant environment variables (see the OSS-Fuzz documentation):
#   LLVM               kbuild toolchain selector: 1, -<suffix> or a
#                      path ending in /; autodetected when unset
#   SANITIZER          address (default), undefined, coverage, none
#   CFLAGS             compiler flags, replacing the per-sanitizer defaults
#   LIB_FUZZING_ENGINE fuzzing engine link argument
#   WORK               scratch space for the build tree
#   OUT                output directory for fuzzers and seed corpora
#   CORPORA            checkout of barebox-fuzz-corpora

set -eux

srctree=$(readlink -f "$(dirname "$0")/..")

# Kbuild's LLVM=1 expects an unsuffixed LLVM toolchain in PATH, which
# not all distributions provide: Debian, for example, may ship clang-21
# and ld.lld-21 without an unsuffixed ld.lld. When LLVM is not given,
# fall back to the newest versioned toolchain that is complete.
llvm_complete() {
	command -v "clang$1" && command -v "ld.lld$1" && \
		command -v "llvm-ar$1"
} >/dev/null

if [ -z "${LLVM:-}" ]; then
	LLVM=1
	if ! llvm_complete ""; then
		for v in $(compgen -c clang- | \
			   sed -n 's/^clang-\([0-9]\+\)$/\1/p' | sort -run); do
			if llvm_complete "-$v"; then
				LLVM=-$v
				break
			fi
		done
	fi
fi

# Default CC/CXX to the same toolchain LLVM= selects for kbuild, so
# auxiliary uses like scripts/clang-runtime-dir.sh match the compiler
# that performs the link
case "$LLVM" in
*/) llvm_prefix=$LLVM llvm_suffix='' ;;
-*) llvm_prefix='' llvm_suffix=$LLVM ;;
*)  llvm_prefix='' llvm_suffix='' ;;
esac
export CC=${CC:-${llvm_prefix}clang$llvm_suffix}
export CXX=${CXX:-${llvm_prefix}clang++$llvm_suffix}
SANITIZER=${SANITIZER:-address}
WORK=${WORK:-$srctree/oss-fuzz-work}
OUT=${OUT:-$WORK/out}
CORPORA=${CORPORA:-${SRC:-$(dirname "$srctree")}/barebox-fuzz-corpora}
BUILD=$WORK/build

mkdir -p "$WORK" "$OUT"

# Outside the OSS-Fuzz containers, provide sane per-sanitizer defaults
if [ -z "${CFLAGS:-}" ]; then
	case "$SANITIZER" in
	address)
		CFLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=address"
		;;
	undefined)
		CFLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=undefined"
		CFLAGS="$CFLAGS -fno-sanitize-recover=undefined"
		;;
	coverage)
		# BUILD_BUG_ON(!__builtin_constant_p(...)) requires constant
		# folding, so -O0 builds are not possible. OSS-Fuzz uses -O1
		# for all sanitizers including coverage.
		CFLAGS="-O1 -g -fno-omit-frame-pointer"
		CFLAGS="$CFLAGS -fprofile-instr-generate -fcoverage-mapping"
		;;
	*)
		CFLAGS="-O1 -g"
		;;
	esac
fi

kmake() {
	make -C "$srctree" O="$BUILD" LLVM="$LLVM" "$@"
}

kmake libfuzzer_defconfig

cfg() {
	"$srctree"/scripts/config --file "$BUILD/.config" "$@"
}

# libfuzzer_defconfig enables both ASAN and UBSAN, but OSS-Fuzz builds
# one sanitizer at a time and provides its flags via CFLAGS. Keep only
# the Kconfig option matching the requested sanitizer: CONFIG_ASAN also
# enables the sandbox stack-switching annotations that ASan needs to
# avoid false positives, and CONFIG_UBSAN's alignment exemption covers
# barebox's intentional unaligned accesses.
case "$SANITIZER" in
address)
	cfg -d UBSAN
	;;
undefined)
	cfg -d ASAN
	;;
memory)
	echo "MSan is not supported: sandbox links the host libc" >&2
	exit 1
	;;
*)
	cfg -d ASAN -d UBSAN
	;;
esac

kmake olddefconfig

# Linking with -fsanitize=fuzzer would drag in libFuzzer's string
# function interceptors, whose strong definitions of memcmp & friends
# collide with barebox's own. Substitute the plain libFuzzer runtime
# archive, which contains the driver, but not the interceptors.
if [ "${LIB_FUZZING_ENGINE:-}" = "-fsanitize=fuzzer" ]; then
	rundir=$("$srctree"/scripts/clang-runtime-dir.sh "$CC")
	for archive in "$rundir/libclang_rt.fuzzer.a" \
		       "$rundir/libclang_rt.fuzzer-$(uname -m).a"; do
		if [ -e "$archive" ]; then
			export LIB_FUZZING_ENGINE="$archive"
			break
		fi
	done

	if [ "$LIB_FUZZING_ENGINE" = "-fsanitize=fuzzer" ]; then
		echo "libclang_rt.fuzzer archive not found in $rundir" >&2
		exit 1
	fi
fi

# KCFLAGS is appended after all barebox flags, so the sanitizer and
# coverage instrumentation flags requested by OSS-Fuzz win. The link
# needs them too (to pull in the sanitizer/profile runtimes), which
# works by exporting BAREBOX_LDFLAGS into the environment: the sandbox
# Makefile appends to it. LIB_FUZZING_ENGINE, when set, is consumed by
# arch/sandbox/Makefile.
export BAREBOX_LDFLAGS="$CFLAGS"
kmake KCFLAGS="$CFLAGS" -j"$(nproc)"

# Enumerate the fuzz targets the build actually produced. The binary
# itself is authoritative; fall back to the fuzz-* symlinks in images/
# if it cannot be executed here.
targets=$("$BUILD/images/barebox" --list-fuzzers) || targets=
if [ -z "$targets" ]; then
	targets=$(find "$BUILD/images" -maxdepth 1 -name 'fuzz-*' \
			-printf '%f\n' | sed 's/^fuzz-//')
fi
if [ -z "$targets" ]; then
	echo "no fuzz targets found in $BUILD/images" >&2
	exit 1
fi

# Targets whose inputs are bigger or hungrier than what the libFuzzer
# defaults (4 KiB inputs, 2 GB RSS) accommodate. Everything not listed
# here is fine with the defaults and gets no .options file.
target_options() {
	case "$1" in
	dtb|dtb-overlay|fdt-compatible)
		# device trees, unflattened into a node/property tree
		echo "rss_limit_mb = 10000"
		echo "max_len = 51200"
		;;
	fit)
		# FIT images embed kernel, initrd and device tree
		echo "rss_limit_mb = 20000"
		echo "max_len = 128000"
		;;
	esac
}

# All fuzzers are the same executable, which selects the fuzz target
# by the basename it is invoked with. Install one copy per target:
# OSS-Fuzz repacks $OUT per target, so symlinks or hardlinks would not
# survive. Seed corpora are zipped from the barebox-fuzz-corpora
# checkout, one flat archive per target that has one. Options are
# written in the ini-like format OSS-Fuzz reads.
for t in $targets; do
	cp -L "$BUILD/images/barebox" "$OUT/fuzz-$t"
	if [ -d "$CORPORA/$t" ]; then
		rm -f "$OUT/fuzz-${t}_seed_corpus.zip"
		# xargs -r skips zip for corpus directories without seeds yet
		find "$CORPORA/$t" -maxdepth 1 -type f -print0 | \
			xargs -0 -r zip -j -q "$OUT/fuzz-${t}_seed_corpus.zip"
	fi
	rm -f "$OUT/fuzz-$t.options"
	options=$(target_options "$t")
	if [ -n "$options" ]; then
		printf '[libfuzzer]\n%s\n' "$options" > "$OUT/fuzz-$t.options"
	fi
done
