#
# Copyright (C) 2013-2016 OpenWrt.org
# Copyright (C) 2016 Yousong Zhou
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/image.mk

FAT32_BLOCK_SIZE=1024
SD_BOOT_PARTSIZE=10
FAT32_BLOCKS=$(shell echo $$(($(SD_BOOT_PARTSIZE)*1024*1024/$(FAT32_BLOCK_SIZE))))

KERNEL_LOADADDR:=0x85000000

DEVICE_VARS += UBOOT BOOT_SCRIPT

define Build/sunxi-sdcard
	rm -f $@.boot
	mkfs.fat $@.boot -C $(FAT32_BLOCKS)

	mcopy -i $@.boot $(KDIR)/image-$(firstword $(DEVICE_DTS)).dtb ::dtb
	mcopy -i $@.boot $(IMAGE_KERNEL) ::uImage
	./gen_imx6ull_sdcard_img.sh $@ \
		$@.boot \
		$(IMAGE_ROOTFS) \
		$(SD_BOOT_PARTSIZE) \
		$(CONFIG_TARGET_ROOTFS_PARTSIZE) \
		$(BIN_DIR)/u-boot-$(UBOOT)/u-boot-dtb.imx
	rm -f $@.boot
endef

define Build/imx6ull-ubootimg
	rm -f $@
	rm -f $@.pad
	dd if=/dev/zero of=$@.pad bs=1024 count=1
	cat $@.pad $(BIN_DIR)/u-boot-$(UBOOT)/u-boot-dtb.imx >$@
endef

define Build/imx6ull-mtd-full
	mv $@ $@.fw	
	$(call Build/imx6ull-ubootimg)
	dd if=/dev/zero of=$@.new bs=1M count=1
	dd if=$@ of=$@.new conv=notrunc	
	cat $@.fw >>$@.new	
	mv $@.new $@
	rm -f $@.new
endef

define Build/imx6ull-mtd
  mv $@ $@.fw
  dd if=$@ of=$@.new conv=notrunc 
  cat $@.fw >>$@.new  
  mv $@.new $@
  rm -f $@.new
endef

# why \x00\x00\x00\x00 for zImage-initramfs
define Device/Default
  PROFILES := Generic
  FILESYSTEMS := squashfs ext4
  KERNEL_INSTALL := 1
  KERNEL_SUFFIX := -uImage
  KERNEL_NAME := zImage
  KERNEL_PREFIX := $$(IMAGE_PREFIX)
  DEVICE_DTS_DIR := ../dts
endef

define Device/somlabs_spi
  DEVICE_TITLE := somlabs-spi
  DEVICE_DTS := imx6ull_somlabs_visionsom-6ull-emmc
  UBOOT := somlabs_visionsom_6ull
  KERNEL := kernel-bin | append-dtb | uImage none
  IMAGE_SIZE := 15000k
  KERNEL_SIZE := 4096k
  IMAGES := mtd-full.bin
  IMAGE/mtd-full.bin := append-kernel | append-rootfs | pad-rootfs | imx6ull-mtd-full
  DEVICE_PACKAGES := u-boot-somlabs_visionsom_6ull 
endef
TARGET_DEVICES += somlabs_spi

define Device/somlabs_sd
  DEVICE_TITLE := somlabs-sd
  DEVICE_DTS := imx6ull_somlabs_visionsom-6ull-emmc
  UBOOT := teleofis_rt50
  KERNEL := kernel-bin | uImage none
  IMAGES := sdcard.img.gz
  IMAGE/sdcard.img.gz := sunxi-sdcard | append-metadata | gzip
  DEVICE_PACKAGES := u-boot-teleofis_rt50 \
  kmod-can kmod-can-mcp251xfd kmod-pps kmod-pps-gpio kmod-usb-serial \
  kmod-usb-serial-option kmod-usb-net-rndis kmod-usb-net-qmi-wwan kmod-usb-serial-cp210x \
  kmod-usb-storage kmod-usb-storage-extras kmod-usb-acm \
  luci luci-theme-teleofis luci-proto-3g luci-proto-qmi luci-proto-gre luci-proto-ip luci-i18n-base-en \
  luci-i18n-firewall-en luci-i18n-opkg-en luci-i18n-base-ru luci-i18n-firewall-ru luci-i18n-opkg-ru \
  simman2 luci-app-simman2 luci-i18n-simman2-en luci-i18n-simman2-ru \
  ttyd luci-app-ttyd luci-i18n-ttyd-en luci-i18n-ttyd-ru \
  pingcontrol luci-app-pingcontrol luci-i18n-pingconrtol-en luci-i18n-pingconrtol-ru \
  pollmydevice luci-app-pollmydevice luci-i18n-pollmydevice-en luci-i18n-pollmydevice-ru \
  smstools3 smscontrol luci-app-smscontrol luci-i18n-smscontrol-en luci-i18n-smscontrol-ru \
  report luci-app-report luci-i18n-report-en luci-i18n-report-ru \
  openvpn-mbedtls luci-app-openvpn luci-i18n-openvpn-en luci-i18n-openvpn-ru \
  luci-app-uhttpd luci-i18n-uhttpd-en luci-i18n-uhttpd-ru \
  strongswan strongswan-default luci-app-strongswan luci-i18n-strongswan-en luci-i18n-strongswan-ru \
  mwan3 luci-app-mwan3 luci-i18n-mwan3-en luci-i18n-mwan3-ru \
  base-files-common \
  htop iperf3 nano picocom zram-swap stm32flash i2c-tools gpsd gpsd-clients xl2tpd \
  ntpd ntp-utils \
  block-mount kmod-fs-ext4 \
  -ip-tiny ip-full 
endef
TARGET_DEVICES += somlabs_sd

define Device/teleofis_rt50_full
  DEVICE_TITLE := teleofis rt50 full
  DEVICE_DTS := imx6ull_teleofis_rt50
  UBOOT := teleofis_rt50
  KERNEL := kernel-bin | append-dtb | uImage none
  IMAGE_SIZE := 31744k
  KERNEL_SIZE := 4096k
  IMAGES := mtd-full.bin
  IMAGE/mtd-full.bin := append-kernel | append-rootfs | pad-rootfs | imx6ull-mtd-full
  DEVICE_PACKAGES := u-boot-teleofis_rt50 \
  kmod-can kmod-pps kmod-pps-gpio kmod-usb-serial \
  kmod-usb-serial-option kmod-usb-net-rndis kmod-usb-net-qmi-wwan kmod-usb-serial-cp210x \
  kmod-usb-storage kmod-usb-storage-extras kmod-usb-acm \
  luci luci-theme-teleofis luci-proto-3g luci-proto-qmi luci-proto-gre luci-proto-ip luci-i18n-base-en \
  luci-i18n-firewall-en luci-i18n-opkg-en luci-i18n-base-ru luci-i18n-firewall-ru luci-i18n-opkg-ru \
  simman2 luci-app-simman2 luci-i18n-simman2-en luci-i18n-simman2-ru \
  ttyd luci-app-ttyd luci-i18n-ttyd-en luci-i18n-ttyd-ru \
  pingcontrol luci-app-pingcontrol luci-i18n-pingconrtol-en luci-i18n-pingconrtol-ru \
  pollmydevice luci-app-pollmydevice luci-i18n-pollmydevice-en luci-i18n-pollmydevice-ru \
  smstools3 smscontrol luci-app-smscontrol luci-i18n-smscontrol-en luci-i18n-smscontrol-ru \
  report luci-app-report luci-i18n-report-en luci-i18n-report-ru \
  openvpn-mbedtls luci-app-openvpn luci-i18n-openvpn-en luci-i18n-openvpn-ru \
  mwan3 luci-app-mwan3 luci-i18n-mwan3-en luci-i18n-mwan3-ru \
  base-files-common \
  htop iperf3 nano picocom zram-swap i2c-tools \
  block-mount kmod-fs-ext4
endef
TARGET_DEVICES += teleofis_rt50_full

define Device/teleofis_rt50
  DEVICE_TITLE := teleofis rt50
  DEVICE_DTS := imx6ull_teleofis_rt50
  UBOOT := teleofis_rt50
  KERNEL_SIZE := 4096k
  KERNEL := kernel-bin | append-dtb | uImage none | pad-to $$(KERNEL_SIZE)
  IMAGE_SIZE := 31744k  
  IMAGES := sysupgrade.bin
  IMAGE/sysupgrade.bin := append-kernel | append-rootfs | pad-rootfs | \
  append-metadata | check-size
  DEVICE_PACKAGES := u-boot-teleofis_rt50 \
  kmod-can kmod-pps kmod-pps-gpio kmod-usb-serial \
  kmod-usb-serial-option kmod-usb-net-rndis kmod-usb-net-qmi-wwan kmod-usb-serial-cp210x \
  kmod-usb-storage kmod-usb-storage-extras kmod-usb-acm \
  luci luci-theme-teleofis luci-proto-3g luci-proto-qmi luci-proto-gre luci-proto-ip luci-i18n-base-en \
  luci-i18n-firewall-en luci-i18n-opkg-en luci-i18n-base-ru luci-i18n-firewall-ru luci-i18n-opkg-ru \
  simman2 luci-app-simman2 luci-i18n-simman2-en luci-i18n-simman2-ru \
  ttyd luci-app-ttyd luci-i18n-ttyd-en luci-i18n-ttyd-ru \
  pingcontrol luci-app-pingcontrol luci-i18n-pingconrtol-en luci-i18n-pingconrtol-ru \
  pollmydevice luci-app-pollmydevice luci-i18n-pollmydevice-en luci-i18n-pollmydevice-ru \
  smstools3 smscontrol luci-app-smscontrol luci-i18n-smscontrol-en luci-i18n-smscontrol-ru \
  report luci-app-report luci-i18n-report-en luci-i18n-report-ru \
  openvpn-mbedtls luci-app-openvpn luci-i18n-openvpn-en luci-i18n-openvpn-ru \
  mwan3 luci-app-mwan3 luci-i18n-mwan3-en luci-i18n-mwan3-ru \
  base-files-common \
  htop iperf3 nano picocom zram-swap i2c-tools \
  block-mount kmod-fs-ext4
endef
TARGET_DEVICES += teleofis_rt50

define Device/teleofis_rt50_sd
  DEVICE_TITLE := teleofis rt50 sd
  DEVICE_DTS := imx6ull_teleofis_rt50
  UBOOT := teleofis_rt50
  KERNEL := kernel-bin | uImage none
  IMAGES := sdcard.img.gz
  IMAGE/sdcard.img.gz := sunxi-sdcard | append-metadata | gzip
  DEVICE_PACKAGES := u-boot-teleofis_rt50 \
  kmod-can kmod-pps kmod-pps-gpio kmod-usb-serial \
  kmod-usb-serial-option kmod-usb-net-rndis kmod-usb-net-qmi-wwan kmod-usb-serial-cp210x \
  kmod-usb-storage kmod-usb-storage-extras kmod-usb-acm \
  luci luci-theme-teleofis luci-proto-3g luci-proto-qmi luci-proto-gre luci-proto-ip luci-i18n-base-en \
  luci-i18n-firewall-en luci-i18n-opkg-en luci-i18n-base-ru luci-i18n-firewall-ru luci-i18n-opkg-ru \
  simman2 luci-app-simman2 luci-i18n-simman2-en luci-i18n-simman2-ru \
  ttyd luci-app-ttyd luci-i18n-ttyd-en luci-i18n-ttyd-ru \
  pingcontrol luci-app-pingcontrol luci-i18n-pingconrtol-en luci-i18n-pingconrtol-ru \
  pollmydevice luci-app-pollmydevice luci-i18n-pollmydevice-en luci-i18n-pollmydevice-ru \
  smstools3 smscontrol luci-app-smscontrol luci-i18n-smscontrol-en luci-i18n-smscontrol-ru \
  report luci-app-report luci-i18n-report-en luci-i18n-report-ru \
  openvpn-mbedtls luci-app-openvpn luci-i18n-openvpn-en luci-i18n-openvpn-ru \
  luci-app-uhttpd luci-i18n-uhttpd-en luci-i18n-uhttpd-ru \
  strongswan strongswan-default luci-app-strongswan luci-i18n-strongswan-en luci-i18n-strongswan-ru \
  mwan3 luci-app-mwan3 luci-i18n-mwan3-en luci-i18n-mwan3-ru \
  base-files-common \
  htop iperf3 nano picocom zram-swap stm32flash i2c-tools gpsd gpsd-clients xl2tpd \
  ntpd ntp-utils \
  block-mount kmod-fs-ext4 \
  -ip-tiny ip-full 
endef
TARGET_DEVICES += teleofis_rt50_sd

$(eval $(call BuildImage))
