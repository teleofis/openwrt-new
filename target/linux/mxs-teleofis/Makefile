include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=mxs-teleofis
BOARDNAME:=Freescale i.MX28 Teleofis
FEATURES:=ext4 rtc usb gpio nand ubifs usbgadget
CPU_TYPE:=arm926ej-s

KERNEL_PATCHVER:=5.4
KERNELNAME:=zImage dtbs

define Target/Description
	Support for Freescale i.MX28 Teleofis boards
endef

include $(INCLUDE_DIR)/target.mk

$(eval $(call BuildTarget))
