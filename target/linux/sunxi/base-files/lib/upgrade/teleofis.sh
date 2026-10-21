# SPDX-License-Identifier: GPL-2.0-only

teleofis_ab_running_slot() {
	local slot="$(cmdline_get_var ab_slot)"

	case "$slot" in
	a|b) echo "$slot" ;;
	esac
}

teleofis_ab_other_slot() {
	case "$1" in
	a) echo b ;;
	b) echo a ;;
	esac
}

teleofis_ab_slot_parts() {
	case "$1" in
	a) echo "1 2" ;;
	b) echo "3 4" ;;
	esac
}

teleofis_ab_check_image() {
	local diskdev

	[ -n "$(teleofis_ab_running_slot)" ] || {
		v "Running slot is unknown, the system was not booted by the A/B U-Boot"
		return 1
	}

	[ -n "$(fw_printenv -n ab_slot 2>/dev/null)" ] || {
		v "U-Boot environment is not readable, check /etc/fw_env.config"
		return 1
	}

	export_bootdevice && export_partdevice diskdev 0 || {
		v "Unable to determine upgrade device"
		return 1
	}

	get_partitions "/dev/$diskdev" bootdisk

	get_image_dd "$1" of=/tmp/image.bs count=1 bs=512b

	get_partitions /tmp/image.bs image

	if [ "$(cat /tmp/partmap.image)" != "$(cat /tmp/partmap.bootdisk)" ] ||
	   [ "$(wc -l < /tmp/partmap.image)" -ne 4 ]; then
		v "Partition layout of the image differs from the disk, A/B upgrade is impossible"
		rm -f /tmp/image.bs /tmp/partmap.bootdisk /tmp/partmap.image
		return 1
	fi

	rm -f /tmp/image.bs /tmp/partmap.bootdisk /tmp/partmap.image
	return 0
}

teleofis_ab_copy_config() {
	local dev="$1"
	local offset count src dst

	[ "$(dd if="/dev/$dev" bs=4 count=1 2>/dev/null)" = "hsqs" ] || {
		v "No squashfs on /dev/$dev"
		return 1
	}

	set -- $(dd if="/dev/$dev" bs=8 skip=5 count=1 2>/dev/null | hexdump -v -e '2/4 "%u "')
	[ "$2" = "0" ] || return 1

	offset=$((($1 + 65535) / 65536 * 128))
	count=$((($(wc -c < "$UPGRADE_BACKUP") + 511) / 512))

	v "Writing configuration to /dev/$dev..."
	dd if="$UPGRADE_BACKUP" of="/dev/$dev" bs=512 seek="$offset" conv=sync,fsync 2>/dev/null

	sync
	echo 3 > /proc/sys/vm/drop_caches

	src="$(dd if="$UPGRADE_BACKUP" bs=512 conv=sync 2>/dev/null | md5sum)"
	dst="$(dd if="/dev/$dev" bs=512 skip="$offset" count="$count" 2>/dev/null | md5sum)"
	[ "$src" = "$dst" ]
}

teleofis_ab_do_upgrade() {
	local image="$1"
	local running target envslot diskdev kdev rdev
	local part start size dev src dst

	running="$(teleofis_ab_running_slot)"
	target="$(teleofis_ab_other_slot "$running")"
	[ -n "$target" ] || {
		v "Running slot is unknown, upgrade aborted"
		return 1
	}

	envslot="$(fw_printenv -n ab_slot 2>/dev/null)"
	[ -n "$envslot" ] || {
		v "U-Boot environment is not readable, upgrade aborted"
		return 1
	}

	export_bootdevice && export_partdevice diskdev 0 || {
		v "Unable to determine upgrade device"
		return 1
	}

	set -- $(teleofis_ab_slot_parts "$target")
	export_partdevice kdev "$1" && export_partdevice rdev "$2" || {
		v "Unable to find partitions of slot $target"
		return 1
	}

	if [ "$envslot" != "$running" ] ||
	   [ -n "$(fw_printenv -n upgrade_available 2>/dev/null)" ]; then
		v "Pinning U-Boot to the running slot $running"
		fw_setenv -s - <<-EOF || return 1
			ab_slot $running
			upgrade_available
			bootcount
		EOF
	fi

	get_image_dd "$image" of=/tmp/image.bs count=1 bs=512b
	get_partitions /tmp/image.bs image

	while read part start size; do
		case "$part" in
		1) dev="$kdev" ;;
		2) dev="$rdev" ;;
		*) continue ;;
		esac

		[ "$(cat "/sys/class/block/$dev/size")" = "$size" ] || {
			v "Size of /dev/$dev differs from image partition $part, upgrade aborted"
			return 1
		}

		v "Writing image partition $part to /dev/$dev (slot $target)..."
		get_image_dd "$image" of="/dev/$dev" iflag=fullblock ibs=512 obs=1M \
			skip="$start" count="$size" conv=fsync
	done < /tmp/partmap.image

	sync
	echo 3 > /proc/sys/vm/drop_caches

	while read part start size; do
		case "$part" in
		1) dev="$kdev" ;;
		2) dev="$rdev" ;;
		*) continue ;;
		esac

		v "Verifying /dev/$dev..."
		src="$(get_image_dd "$image" iflag=fullblock ibs=512 skip="$start" count="$size" | md5sum)"
		dst="$(dd if="/dev/$dev" bs=512 count="$size" 2>/dev/null | md5sum)"
		[ "$src" = "$dst" ] || {
			v "Verification of /dev/$dev failed, slot $running stays active"
			return 1
		}
	done < /tmp/partmap.image

	if [ -n "$UPGRADE_BACKUP" ]; then
		teleofis_ab_copy_config "$rdev" || {
			v "Saving configuration to slot $target failed, slot $running stays active"
			return 1
		}
	fi

	v "Switching U-Boot to slot $target"
	fw_setenv -s - <<-EOF || return 1
		ab_slot $target
		upgrade_available 1
		bootcount 0
	EOF
}
