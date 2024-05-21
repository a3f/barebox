#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# Output json formatted defconfig list for Github Action consumption
ARCH=${@:-$(for a in arch/*/; do basename $a; done)}

echo '{ "include" : [ '
for arch in $ARCH; do
	make ARCH=$arch CROSS_COMPILE= help | \
		awk '/_defconfig/ { print $1  }' | \
		xargs -i printf '{ "arch": "%s", "config": "%s" }\n' \
		"$arch" "{}" | \
		paste -sd ',' -
done | paste -sd ',' -
echo '] }'
