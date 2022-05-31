UBIFS_OPTS = -m 2048 -e 124KiB -c 2000
DEVICE_VARS += DTS UBIFS_OPTS
KERNEL_LOADADDR := 0x40008000

define Device/Default
  KERNEL_NAME := zImage
  KERNEL_SUFFIX := -uImage
  KERNEL_INSTALL := 1
  BLOCKSIZE := 128k
  PAGESIZE := 2048
  MKUBIFS_OPTS := -m $$(PAGESIZE) -e 124KiB
  SUBPAGESIZE := 512
  FILESYSTEMS := ubifs
  PROFILES = Default $$(DTS)
  SUPPORTED_DEVICES := $(subst _,$(comma),$(1))
  DEVICE_DTS_DIR := ../dts
  DEVICE_DTS := imx28-$(subst _,-,$(1))
  KERNEL_SIZE := 5120k
  KERNEL := kernel-bin | uImage none | pad-to 4096k | append-dtb 
endef

define Device/teleofis_rtux68v2
  DEVICE_VENDOR := Teleofis
  DEVICE_MODEL := RTUx68v2
  SUPPORTED_DEVICES := teleofis,rtux68v2
  DEVICE_DTS:= imx28_teleofis_rtux68v2
  DEVICE_PACKAGES := kmod-pps kmod-pps-ldisc kmod-pps-gpio \
  kmod-usb-serial kmod-usb-serial-ftdi kmod-usb-serial-cp210x \
  kmod-usb-serial-ch341 kmod-usb-serial-option kmod-usb-net-rndis kmod-usb-net-qmi-wwan \
  kmod-input-gpio-keys kmod-rtl8192cu \
  kmod-usb-storage kmod-usb-storage-extras kmod-usb-acm \
  kmod-fs-exfat kmod-fs-ext4 kmod-fs-f2fs kmod-fs-vfat kmod-fs-ntfs kmod-fs-nfs \
  luci luci-theme-teleofis luci-proto-3g luci-proto-qmi luci-proto-gre luci-proto-ip luci-i18n-base-en \
  luci-i18n-firewall-en luci-i18n-opkg-en luci-i18n-base-ru luci-i18n-firewall-ru luci-i18n-opkg-ru \
  ttyd luci-app-ttyd luci-i18n-ttyd-en luci-i18n-ttyd-ru \
  pingcontrol luci-app-pingcontrol luci-i18n-pingconrtol-en luci-i18n-pingconrtol-ru \
  pollmydevice luci-app-pollmydevice luci-i18n-pollmydevice-en luci-i18n-pollmydevice-ru \
  smstools3 smscontrol luci-app-smscontrol luci-i18n-smscontrol-en luci-i18n-smscontrol-ru \
  report luci-app-report luci-i18n-report-en luci-i18n-report-ru \
  openvpn-mbedtls luci-app-openvpn luci-i18n-openvpn-en luci-i18n-openvpn-ru \
  luci-app-uhttpd luci-i18n-uhttpd-en luci-i18n-uhttpd-ru \
  strongswan strongswan-default luci-app-strongswan luci-i18n-strongswan-en luci-i18n-strongswan-ru \
  mwan3 luci-app-mwan3 luci-i18n-mwan3-en luci-i18n-mwan3-ru \
  base-files-common base-files-rtux68 \
  simman2 luci-app-simman2 luci-i18n-simman2-en luci-i18n-simman2-ru \
  iolines luci-app-iolines luci-i18n-iolines-en luci-i18n-iolines-ru \
  htop iperf3 nano picocom zram-swap stm32flash gpsd gpsd-clients xl2tpd bc \
  ntpd ntp-utils uboot-envtools block-mount coreutils coreutils-sleep tcpdump \
  collectd collectd-mod-df collectd-mod-interface collectd-mod-load \
  collectd-mod-uptime collectd-mod-processes collectd-mod-network \
  collectd-mod-cpu collectd-mod-cpufreq collectd-mod-memory collectd-mod-ping \
  collectd-mod-thermal collectd-mod-exec \
  lsof wpad hostapd snmpd python3 python3-pip iconv pps-tools
  IMAGES := nand.ubi sysupgrade.tar 
  IMAGE_NAME = $$(IMAGE_PREFIX)-$$(1).$$(2)
  IMAGE/nand.ubi := append-ubi
  IMAGE/sysupgrade.tar := sysupgrade-tar | append-metadata
  IMAGE_NAME = $$(IMAGE_PREFIX)-$$(1)-$$(2)
endef
TARGET_DEVICES += teleofis_rtux68v2