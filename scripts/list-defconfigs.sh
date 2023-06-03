#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# Output json formatted defconfig list for Github Action consumption

ARCH=${@:-*}

archs=$(for arch in arch/$ARCH; do
	arch=$(basename $arch)

	cfgs=$(ls -1 arch/$arch/configs)

	for cfg in $cfgs; do
		lgenv=''

		yaml=test/$arch/$cfg.yaml
		if [ -L $yaml ]; then
			lgenv=', "lgenv": "'"test/$arch/*@$cfg.yaml"'"'
		elif [ -f $yaml ]; then
			lgenv=', "lgenv": "'"$yaml"'"'
		fi
		printf '{ "arch": "%s", "config": "%s" %s }\n' "$arch" "$cfg" "$lgenv"
	done | paste -sd ',' -
done | paste -sd ',' -)

echo '{ "include" : '" [ $archs ] }"
