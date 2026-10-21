#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# SD card image with two firmware slots (A/B):
#
#   8 KiB          SPL + U-Boot
#   1 MiB          U-Boot environment, redundant copy at 1.125 MiB
#   4 MiB  p1      slot A: kernel FIT, raw
#          p2      slot A: squashfs rootfs, overlay is created behind it
#          p3, p4  slot B: the same as p1, p2
#
# Both slots get the same firmware. The whole disk area is part of the
# image, so leftovers on the card can not be taken for an overlay.

set -ex
[ $# -eq 7 ] || {
    echo "SYNTAX: $0 <file> <kernel image> <rootfs image> <kernel size> <rootfs size> <u-boot image> <disk signature>"
    exit 1
}

OUTPUT="$1"
KERNEL="$2"
ROOTFS="$3"
KERNELSIZE="$4"
ROOTFSSIZE="$5"
UBOOT="$6"
SIGNATURE="$7"

head=4
sect=63

set $(ptgen -o $OUTPUT -h $head -s $sect -l 4096 -S 0x$SIGNATURE \
	-t da -p ${KERNELSIZE}M -t 83 -p ${ROOTFSSIZE}M \
	-t da -p ${KERNELSIZE}M -t 83 -p ${ROOTFSSIZE}M)

[ "$(wc -c < "$KERNEL")" -le "$2" ] || { echo "Kernel does not fit into ${KERNELSIZE}M"; exit 1; }
[ "$(wc -c < "$ROOTFS")" -le "$4" ] || { echo "Rootfs does not fit into ${ROOTFSSIZE}M"; exit 1; }

KERNEL_A_OFFSET="$(($1 / 512))"
ROOTFS_A_OFFSET="$(($3 / 512))"
KERNEL_B_OFFSET="$(($5 / 512))"
ROOTFS_B_OFFSET="$(($7 / 512))"
DISK_END="$((($7 + $8) / 512))"

dd bs=1024 if="$UBOOT" of="$OUTPUT" seek=8 conv=notrunc
dd bs=512 if="$KERNEL" of="$OUTPUT" seek="$KERNEL_A_OFFSET" conv=notrunc
dd bs=512 if="$ROOTFS" of="$OUTPUT" seek="$ROOTFS_A_OFFSET" conv=notrunc
dd bs=512 if="$KERNEL" of="$OUTPUT" seek="$KERNEL_B_OFFSET" conv=notrunc
dd bs=512 if="$ROOTFS" of="$OUTPUT" seek="$ROOTFS_B_OFFSET" conv=notrunc
dd bs=512 if=/dev/zero of="$OUTPUT" seek="$((DISK_END - 1))" count=1 conv=notrunc
