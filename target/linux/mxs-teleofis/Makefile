#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=mxs-teleofis
BOARDNAME:=Freescale i.MX23/i.MX28 Teleofis
FEATURES:=ext4 rtc usb gpio nand ubifs
CPU_TYPE:=arm926ej-s

KERNEL_PATCHVER:=4.14
KERNELNAME:=zImage dtbs

define Target/Description
	Support for Freescale i.MX23/i.MX28 Teleofis boards
endef

include $(INCLUDE_DIR)/target.mk

$(eval $(call BuildTarget))
