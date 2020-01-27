UBIFS_OPTS = -m 2048 -e 124KiB -c 2000
DEVICE_VARS += DTS UBIFS_OPTS
KERNEL_LOADADDR := 0x40008000

define Build/sysupgrade-tar-dtb
  ./sysupgrade-tar.sh \
    --board $(if $(BOARD_NAME),$(BOARD_NAME),$(DEVICE_NAME)) \
    --kernel $(call param_get_default,kernel,$(1),$(IMAGE_KERNEL)) \
    --dtb $(call param_get_default,dtb,$(1),$(DTS_DIR)/$(DEVICE_DTS).dtb) \
    --rootfs $(call param_get_default,rootfs,$(1),$(IMAGE_ROOTFS)) \
    $@
endef

define Device/Default
  KERNEL_NAME := zImage
  KERNEL_SUFFIX := -uImage
  KERNEL_INSTALL := 1
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  SUBPAGESIZE := 512
  FILESYSTEMS := ubifs
  PROFILES = Default $$(DTS)
  SUPPORTED_DEVICES := $(subst _,$(comma),$(1))
  DEVICE_DTS := imx28-$(subst _,-,$(1))
  KERNEL := kernel-bin | uImage none
  IMAGES := sysupgrade.tar
  IMAGE/sysupgrade.tar := sysupgrade-tar-dtb
endef

define Device/teleofis
  DEVICE_TITLE := Teleofis RTU968v2 router
  SUPPORTED_DEVICES += teleofis
  DEVICE_PACKAGES := kmod-usb-acm kmod-rtc-pcf8563 \
                    kmod-rtc-ds1307
endef
TARGET_DEVICES += teleofis
