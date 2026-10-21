#!/bin/bash
# Copy the ConfigFix sources into a Linux 7.3 source tree.
TARGET_VERSION="7.3"

force=0
tree=""
for arg in "$@"; do
	case "$arg" in
	--force) force=1 ;;
	*) if [[ -n "$tree" ]]; then tree="" ; break; fi; tree="$arg" ;;
	esac
done

if [[ -z "$tree" ]]; then
	echo "Usage: copy_files.sh [--force] \$KERNEL_SOURCE_TREE"
	exit 1
fi

if [[ ! -d "$tree/scripts/kconfig" ]]; then
	echo "$tree does not look like a Linux source tree." >&2
	exit 1
fi

# These files replace their upstream counterparts, so they only fit the kernel
# version this branch was prepared for. Mixing versions produces build errors
# that look unrelated (e.g. missing 'choice_link' or 'list_count_nodes').
version=$(make -s -C "$tree" kernelversion 2>/dev/null | cut -d. -f1,2 || true)
if [[ -n "$version" && "$version" != "$TARGET_VERSION" ]]; then
	echo "This branch targets Linux $TARGET_VERSION, but $tree is $version." >&2
	if [[ "$force" -ne 1 ]]; then
		echo "Use the copy_patch branch matching your kernel, or pass --force." >&2
		exit 1
	fi
	echo "Continuing anyway (--force)." >&2
fi

cp -r ./scripts/kconfig "$tree/scripts"
cp ./scripts/include/list.h "$tree/scripts/include/list.h"
echo "Files copied successfully. Now run 'make xconfig' in $tree"
