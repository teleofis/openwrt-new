#!/bin/sh

tar xf bin/targets/mxs-teleofis/generic/openwrt-mxs-teleofis-teleofis-ubifs-sysupgrade.tar -C /tftpboot/
mv /tftpboot/sysupgrade-teleofis/fdt	/tftpboot/fdt.dtb
mv /tftpboot/sysupgrade-teleofis/kernel /tftpboot/openwrt-mxs-uImage
mv /tftpboot/sysupgrade-teleofis/root	/tftpboot/rootfs.img
rm  -r /tftpboot/sysupgrade-teleofis

exit 0
