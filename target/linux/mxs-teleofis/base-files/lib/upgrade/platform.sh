#
# Copyright (C) 2010-2015 OpenWrt.org
#

RAMFS_COPY_BIN='jffs2reset fw_printenv fw_setenv mkdir touch'
RAMFS_COPY_DATA='/etc/fw_env.config /var/lock/fw_printenv.lock'

LI_UBIPART="${LI_UBIPART:-root}"
LI_KERNPART_A="${LI_KERNPART_A:-kernel_a}"
LI_KERNPART_B="${LI_KERNPART_B:-kernel_b}"
LI_ROOTPART_A="${LI_ROOTPART_A:-rootfs_a}"
LI_ROOTPART_B="${LI_ROOTPART_B:-rootfs_b}"
LI_KERNPART="${LI_KERNPART_A:-kernel_a}"
LI_ROOTPART="${LI_ROOTPART_A:-rootfs_a}"
LI_IMAGE=$(cat /tmp/sysinfo/rootfs)

vendor_upgrade_prepare_ubi() {
	local ubidev="$( nand_find_ubi "$LI_UBIPART" )"
	local kern_ubivol="$( nand_find_volume $ubidev $LI_KERNPART )"
	local root_ubivol="$( nand_find_volume $ubidev $LI_ROOTPART )"

	# update fixed kernel part
	[ "$kern_ubivol" ] && ubirmvol /dev/$ubidev -N $LI_KERNPART || true
	if ! ubimkvol /dev/$ubidev -N $LI_KERNPART -m; then
		echo "cannot create kernel volume"
		return 1;
	fi

	# update fixed rootfs part
	[ "$root_ubivol" ] && ubirmvol /dev/$ubidev -N $LI_ROOTPART || true
	if ! ubimkvol /dev/$ubidev -N $LI_ROOTPART -m; then
		echo "cannot create rootfs volume"
		return 1;
	fi

	sync
	return 0
}

vendor_do_upgrade() {
	if [ "$LI_IMAGE" == "ubi0:rootfs_a" ]; then
		LI_KERNPART="$LI_KERNPART_B"
		LI_ROOTPART="$LI_ROOTPART_B"
	else
		LI_KERNPART="$LI_KERNPART_A"
		LI_ROOTPART="$LI_ROOTPART_A"
	fi
	local has_rootfs=0
	local has_kernel=0
	local tar_file="$1"
	local rootfs_length
	local kernel_length

	local board_dir=$(tar tf "$tar_file" | grep -m 1 '^sysupgrade-.*/$')
	board_dir=${board_dir%/}

	tar tf "$tar_file" ${board_dir}/kernel 1>/dev/null 2>/dev/null && has_kernel=1
	[ "$has_kernel" = "1" ] && {
		kernel_length=$( (tar xf "$tar_file" ${board_dir}/kernel -O | wc -c) 2> /dev/null)
	}

	tar tf "$tar_file" ${board_dir}/root 1>/dev/null 2>/dev/null && has_rootfs=1
	[ "$has_rootfs" = "1" ] && {
		rootfs_length=$( (tar xf "$tar_file" ${board_dir}/root -O | wc -c) 2> /dev/null)
	}

	vendor_upgrade_prepare_ubi

	local ubidev="$( nand_find_ubi "$LI_UBIPART" )"
	[ "$has_kernel" = "1" ] && {
		local kern_ubivol="$( nand_find_volume $ubidev $LI_KERNPART )"
		tar xf "$tar_file" ${board_dir}/kernel -O | \
			ubiupdatevol /dev/$kern_ubivol -s $kernel_length -
	}

	[ "$has_rootfs" = "1" ] && {
		local root_ubivol="$( nand_find_volume $ubidev $LI_ROOTPART )"
		tar xf "$tar_file" ${board_dir}/root -O | \
			ubiupdatevol /dev/$root_ubivol -s $rootfs_length -
	}
	if [ "$LI_IMAGE" == "ubi0:rootfs_a" ]; then
		fw_setenv image "b"
		fw_setenv boot_status "upgrade"
	else
		fw_setenv image "a"
		fw_setenv boot_status "upgrade"
	fi
	nand_do_upgrade_success
}

platform_check_image() {
	local board=$(board_name)

	case "$board" in
	teleofis,rtux68v2 )
		nand_do_platform_check teleofis_rtux68v2 $1
		return $?;
		;;
	esac

	echo "Sysupgrade is not yet supported on $board."
	return 1
}

platform_do_upgrade() {
	# Force the creation of fw_printenv.lock
	mkdir -p /var/lock
	touch /var/lock/fw_printenv.lock
	local board=$(board_name)
	case "$board" in
	teleofis,rtux68v2 )
		vendor_do_upgrade "$1"
		;;
	esac
}

platform_pre_upgrade() {
	local board=$(board_name)

	case "$board" in
	teleofis,rtux68v2 )
		[ -z "$UPGRADE_BACKUP" ] && {
			jffs2reset -y
			umount /overlay
		}
		;;
	esac
}
