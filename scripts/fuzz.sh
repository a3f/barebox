#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Run every fuzz target for a share of a total time budget:
#
#   ./scripts/fuzz.sh ../barebox-fuzz-corpora 60
#
# builds the fuzzers with scripts/oss-fuzz.sh, then runs each target
# for (60 minutes / number of targets) in libFuzzer's fork mode with
# one worker per CPU, so the whole run takes roughly the given number
# of minutes. Each target fuzzes the corpus subdirectory named after
# it (e.g. dtb/ for fuzz-dtb) and adds newly found inputs there.
#
# Crash reproducers are collected in $ARTIFACTS, prefixed with the
# name of the target that produced them, e.g. fuzz-dtb-crash-<sha1>.
# The script exits non-zero if any reproducers were written.
#
# Per-target libFuzzer options are read from the fuzz-<target>.options
# files that scripts/oss-fuzz.sh installs next to the binaries, in the
# same format OSS-Fuzz uses. Extra arguments are passed through to
# every target and override them:
#
#   ./scripts/fuzz.sh ../barebox-fuzz-corpora 60 -verbosity=2
#
# Environment (all optional; see scripts/oss-fuzz.sh for build knobs):
#   WORK        scratch space, default oss-fuzz-work/ in the source tree
#   OUT         directory with the fuzz-* binaries, default $WORK/out
#   ARTIFACTS   where reproducers go, default $WORK/artifacts
#   JOBS        parallel fuzzing processes per target, default nproc
#   SKIP_BUILD  set to 1 to reuse the binaries already in $OUT

set -eu -o pipefail

usage() {
	echo "usage: $0 CORPORA MINUTES [EXTRA_LIBFUZZER_ARGS...]" >&2
	exit 1
}

[ $# -ge 2 ] || usage

srctree=$(readlink -f "$(dirname "$0")/..")

corpora=$(readlink -f "$1")
minutes=$2
shift 2

case "$minutes" in
''|*[!0-9]*)
	usage ;;
esac
[ "$minutes" -gt 0 ] || usage

if [ ! -d "$corpora" ]; then
	echo "$0: corpus directory $corpora does not exist" >&2
	exit 1
fi

export WORK=${WORK:-$srctree/oss-fuzz-work}
export OUT=${OUT:-$WORK/out}
ARTIFACTS=${ARTIFACTS:-$WORK/artifacts}
JOBS=${JOBS:-$(nproc)}

if [ "${SKIP_BUILD:-0}" != 1 ]; then
	CORPORA=$corpora "$srctree/scripts/oss-fuzz.sh"
fi

targets=()
for f in "$OUT"/fuzz-*; do
	[ -f "$f" ] && [ -x "$f" ] || continue
	targets+=("${f##*/fuzz-}")
done

if [ ${#targets[@]} -eq 0 ]; then
	echo "$0: no fuzz targets found in $OUT" >&2
	exit 1
fi

secs=$(( minutes * 60 / ${#targets[@]} ))
[ "$secs" -ge 1 ] || secs=1

# Emit the [libfuzzer] section of an OSS-Fuzz fuzz-<target>.options
# file as -key=value command line arguments
target_options() {
	local f="$OUT/fuzz-$1.options"

	[ -f "$f" ] || return 0
	sed -n '/^\[libfuzzer\]/,/^\[/{
		s/^[[:space:]]*\([A-Za-z_]\+\)[[:space:]]*=[[:space:]]*/-\1=/p
	}' "$f"
}

mkdir -p "$ARTIFACTS"
logdir=$WORK/logs
mkdir -p "$logdir"
stamp=$(mktemp "$WORK/.fuzz-stamp.XXXXXX")
trap 'rm -f "$stamp"' EXIT
status=0

echo "running ${#targets[@]} fuzz targets for ${secs}s each on $JOBS cores"

for t in "${targets[@]}"; do
	corpus=$corpora/$t
	log=$logdir/fuzz-$t.log
	mkdir -p "$corpus"
	mapfile -t opts < <(target_options "$t")

	echo "=== fuzz-$t (${secs}s, log: $log) ==="

	# Fork mode keeps fuzzing across crashes/OOMs/timeouts and
	# enforces -max_total_time as a wall clock limit for the whole
	# worker pool, so each target consumes exactly its time share.
	if ! (cd "$WORK" && "$OUT/fuzz-$t" -fork="$JOBS" \
			-ignore_crashes=1 -max_total_time="$secs" \
			-artifact_prefix="$ARTIFACTS/fuzz-$t-" \
			-print_final_stats=1 \
			"${opts[@]}" "$@" "$corpus") 2>&1 | tee "$log"; then
		echo "fuzz-$t exited unsuccessfully, see $log" >&2
		status=1
	fi
done

mapfile -t crashes < <(find "$ARTIFACTS" -type f -newer "$stamp" | sort)
if [ ${#crashes[@]} -gt 0 ]; then
	echo "=== ${#crashes[@]} reproducer(s) written ===" >&2
	printf '%s\n' "${crashes[@]}" >&2
	status=1
else
	echo "=== no reproducers written ==="
fi

exit $status
