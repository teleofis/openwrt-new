# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2013-2016 OpenWrt.org
# Copyright (C) 2016 Yousong Zhou

KERNEL_LOADADDR:=0x40008000

define Device/sun50i
  SUNXI_DTS_DIR := allwinner/
  KERNEL_NAME := Image
  KERNEL := kernel-bin
endef

define Device/sun50i-a64
  SOC := sun50i-a64
  $(Device/sun50i)
endef

define Device/sun50i-h5
  SOC := sun50i-h5
  $(Device/sun50i)
endef

define Device/sun50i-h6
  SOC := sun50i-h6
  $(Device/sun50i)
endef

define Device/sun50i-h616
  SOC := sun50i-h616
  $(Device/sun50i)
endef

define Device/sun50i-h618
  SOC := sun50i-h618
  $(Device/sun50i)
endef

define Device/sun50i-t507
  SOC := sun50i-t507
  $(Device/sun50i)
endef

define Device/friendlyarm_nanopi-neo-plus2
  DEVICE_VENDOR := FriendlyARM
  DEVICE_MODEL := NanoPi NEO Plus2
  SUPPORTED_DEVICES:=nanopi-neo-plus2
  $(Device/sun50i-h5)
endef
TARGET_DEVICES += friendlyarm_nanopi-neo-plus2

define Device/friendlyarm_nanopi-neo2
  DEVICE_VENDOR := FriendlyARM
  DEVICE_MODEL := NanoPi NEO2
  SUPPORTED_DEVICES:=nanopi-neo2
  $(Device/sun50i-h5)
endef
TARGET_DEVICES += friendlyarm_nanopi-neo2

define Device/friendlyarm_nanopi-r1s-h5
  DEVICE_VENDOR := FriendlyARM
  DEVICE_MODEL := Nanopi R1S H5
  DEVICE_PACKAGES := kmod-gpio-button-hotplug kmod-usb-net-rtl8152
  SUPPORTED_DEVICES:=nanopi-r1s-h5
  $(Device/sun50i-h5)
endef
TARGET_DEVICES += friendlyarm_nanopi-r1s-h5

define Device/libretech_all-h3-cc-h5
  DEVICE_VENDOR := Libre Computer
  DEVICE_MODEL := ALL-H3-CC
  DEVICE_VARIANT := H5
  $(Device/sun50i-h5)
  SUNXI_DTS := $$(SUNXI_DTS_DIR)$$(SOC)-libretech-all-h3-cc
endef
TARGET_DEVICES += libretech_all-h3-cc-h5

define Device/olimex_a64-olinuxino
  DEVICE_VENDOR := Olimex
  DEVICE_MODEL := A64-Olinuxino
  DEVICE_PACKAGES := kmod-rtl8723bs rtl8723bu-firmware
  $(Device/sun50i-a64)
  SUNXI_DTS := $$(SUNXI_DTS_DIR)$$(SOC)-olinuxino
endef
TARGET_DEVICES += olimex_a64-olinuxino

define Device/olimex_a64-olinuxino-emmc
  DEVICE_VENDOR := Olimex
  DEVICE_MODEL := A64-Olinuxino
  DEVICE_VARIANT := eMMC
  DEVICE_PACKAGES := kmod-rtl8723bs rtl8723bu-firmware
  $(Device/sun50i-a64)
  SUNXI_DTS := $$(SUNXI_DTS_DIR)$$(SOC)-olinuxino-emmc
endef
TARGET_DEVICES += olimex_a64-olinuxino-emmc

define Device/pine64_pine64-plus
  DEVICE_VENDOR := Pine64
  DEVICE_MODEL := Pine64+
  DEVICE_PACKAGES := kmod-rtl8723bs rtl8723bu-firmware
  $(Device/sun50i-a64)
endef
TARGET_DEVICES += pine64_pine64-plus

define Device/pine64_sopine-baseboard
  DEVICE_VENDOR := Pine64
  DEVICE_MODEL := SoPine
  DEVICE_PACKAGES := kmod-rtl8723bs rtl8723bu-firmware
  $(Device/sun50i-a64)
endef
TARGET_DEVICES += pine64_sopine-baseboard

define Device/xunlong_orangepi-one-plus
  $(Device/sun50i-h6)
  DEVICE_VENDOR := Xunlong
  DEVICE_MODEL := Orange Pi One Plus
  SUNXI_DTS_DIR := allwinner/
endef
TARGET_DEVICES += xunlong_orangepi-one-plus

define Device/xunlong_orangepi-pc2
  DEVICE_VENDOR := Xunlong
  DEVICE_MODEL := Orange Pi PC 2
  $(Device/sun50i-h5)
endef
TARGET_DEVICES += xunlong_orangepi-pc2

define Device/xunlong_orangepi-zero2
  DEVICE_VENDOR := Xunlong
  DEVICE_MODEL := Orange Pi Zero 2
  $(Device/sun50i-h616)
endef
TARGET_DEVICES += xunlong_orangepi-zero2

define Device/xunlong_orangepi-zero3
  DEVICE_VENDOR := Xunlong
  DEVICE_MODEL := Orange Pi Zero 3
  $(Device/sun50i-h618)
endef
TARGET_DEVICES += xunlong_orangepi-zero3

define Device/xunlong_orangepi-zero-plus
  DEVICE_VENDOR := Xunlong
  DEVICE_MODEL := Orange Pi Zero Plus
  $(Device/sun50i-h5)
endef
TARGET_DEVICES += xunlong_orangepi-zero-plus

define Device/teleofis_rtu1068v5
  DEVICE_VENDOR := Teleofis
  DEVICE_MODEL := RTU1068v5
  DEVICE_PACKAGES := kmod-pps kmod-pps-ldisc kmod-pps-gpio \
  kmod-usb-serial kmod-usb-serial-ftdi kmod-usb-serial-cp210x \
  kmod-usb-serial-ch341 kmod-usb-serial-option kmod-usb-net-rndis kmod-usb-net-qmi-wwan \
  kmod-button-hotplug kmod-wireguard kmod-soft-uart \
  kmod-usb-storage kmod-usb-storage-extras kmod-usb-acm \
  kmod-fs-exfat kmod-fs-ext4 kmod-fs-f2fs kmod-fs-vfat kmod-fs-ntfs kmod-fs-nfs \
  kmod-crypto-cbc kmod-crypto-ctr kmod-cryptodev kmod-nft-xfrm kmod-ledtrig-tty \
  kmod-l2tp-ip kmod-l2tp-eth \
  luci luci-ssl-openssl luci-theme-teleofis luci-proto-3g luci-proto-qmi \
  luci-proto-nbiot luci-proto-gre luci-proto-ip luci-proto-wireguard \
  luci-i18n-base-en luci-i18n-firewall-en luci-i18n-opkg-en luci-i18n-base-ru \
  luci-i18n-firewall-ru luci-i18n-opkg-ru luci-mod-rpc \
  luci-app-wireguard luci-i18n-wireguard-en luci-i18n-wireguard-ru \
  ddns-scripts wget-ssl bind-host luci-app-ddns luci-i18n-ddns-en luci-i18n-ddns-ru \
  eoip luci-app-eoip luci-i18n-eoip-en luci-i18n-eoip-ru \
  hotplug-generator luci-app-hotplug luci-i18n-hotplug-en luci-i18n-hotplug-ru \
  ttyd luci-app-ttyd luci-i18n-ttyd-en luci-i18n-ttyd-ru \
  pingcontrol luci-app-pingcontrol luci-i18n-pingcontrol-en luci-i18n-pingcontrol-ru \
  pollmydevice luci-app-pollmydevice luci-i18n-pollmydevice-en luci-i18n-pollmydevice-ru \
  pptpd pptpd-ext ppp-mod-pptp luci-app-pptpd luci-i18n-pptpd-en luci-i18n-pptpd-ru \
  smstools3-fix smscontrol luci-app-smscontrol luci-i18n-smscontrol-en luci-i18n-smscontrol-ru \
  report luci-app-report luci-i18n-report-en luci-i18n-report-ru \
  openvpn-openssl luci-app-openvpn luci-i18n-openvpn-en luci-i18n-openvpn-ru \
  luci-app-uhttpd luci-i18n-uhttpd-en luci-i18n-uhttpd-ru \
  luci-app-tinyproxy luci-i18n-tinyproxy-en luci-i18n-tinyproxy-ru \
  strongswan strongswan-charon strongswan-ipsec strongswan-mod-aes strongswan-mod-des \
  strongswan-mod-gmp strongswan-mod-hmac strongswan-mod-kernel-netlink strongswan-mod-rc2 \
  strongswan-mod-sha2 strongswan-mod-md5 strongswan-mod-revocation strongswan-mod-constraints \
  strongswan-mod-pkcs1 strongswan-mod-pgp strongswan-mod-dnskey strongswan-mod-sshkey \
  strongswan-mod-stroke strongswan-mod-updown strongswan-mod-x509 strongswan-mod-xcbc \
  strongswan-mod-pem strongswan-mod-fips-prf strongswan-mod-attr strongswan-mod-resolve \
  strongswan-mod-connmark strongswan-mod-socket-default \
  strongswan-mod-xauth-generic strongswan-mod-vici \
  luci-app-strongswan luci-i18n-strongswan-en luci-i18n-strongswan-ru \
  mwan3 luci-app-mwan3 luci-i18n-mwan3-en luci-i18n-mwan3-ru \
  snmpd-ssl snmp-utils-ssl snmptrapd-ssl \
  luci-app-snmpd-ssl luci-i18n-snmpd-ssl-en luci-i18n-snmpd-ssl-ru \
  xl2tpd xl2tpd-ext luci-app-xl2tpd luci-i18n-xl2tpd-en luci-i18n-xl2tpd-ru \
  base-files-common base-files-rtu1068v5 \
  rms luci-app-rms luci-i18n-rms-en luci-i18n-rms-ru \
  simman2 luci-app-simman2 luci-i18n-simman2-en luci-i18n-simman2-ru \
  iolines-rtu1068v5 iolinesd luci-app-iolines luci-i18n-iolines-en luci-i18n-iolines-ru \
  supervisor \
  htop iperf3 nano picocom zram-swap stm32flash-gd gpsd gpsd-clients bc \
  ntpd ntp-utils uboot-envtools block-mount e2fsprogs exfat-mkfs fdisk f2fs-tools parted tcpdump \
  mosquitto-ssl mosquitto-client-ssl coreutils coreutils-sleep coreutils-stty \
  collectd collectd-mod-df collectd-mod-interface collectd-mod-load \
  collectd-mod-uptime collectd-mod-processes collectd-mod-network \
  collectd-mod-cpu collectd-mod-cpufreq collectd-mod-memory collectd-mod-ping \
  collectd-mod-thermal collectd-mod-exec \
  zabbix-agentd-openssl zabbix-config luci-app-zabbix luci-i18n-zabbix-en luci-i18n-zabbix-ru \
  frr frr-bgpd frr-nhrpd frr-ospfd frr-staticd frr-vtysh frr-watchfrr frr-zebra \
  lsof socat iconv python3 python3-pip python3-pyserial \
  pps-tools i2c-tools usbutils libustream-openssl libopenssl-devcrypto
  $(Device/sun50i-t507)
  FILESYSTEMS := squashfs
  KERNEL_LOADADDR := 0x40200000
  KERNEL := kernel-bin | gzip | fit gzip $$(DTS_DIR)/$$(SUNXI_DTS).dtb
  IMAGE/sdcard.img.gz := sunxi-sdcard-ab | append-metadata | gzip
endef
TARGET_DEVICES += teleofis_rtu1068v5
