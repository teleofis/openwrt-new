#!/bin/sh

[ -n "$INCLUDE_ONLY" ] || {
	NOT_INCLUDED=1
	INCLUDE_ONLY=1

	. ../netifd-proto.sh
	. ./ppp.sh
	. /lib/functions.sh
	init_proto "$@"
}

proto_nbiot_init_config() {
	no_device=1
	available=1
	ppp_generic_init_config
	proto_config_add_string "device:device"
	proto_config_add_string "apn"
	proto_config_add_string "pincode"
	proto_config_add_string "dialnumber"
	proto_config_add_string "band"
	proto_config_add_string "username"
	proto_config_add_string "password"
	proto_config_add_string "auth"
}

modem_reset(){
	echo low > /sys/class/gpio/gpio133/direction
	sleep 3
	echo high > /sys/class/gpio/gpio133/direction
	sleep 2
}

proto_nbiot_setup() {
	local interface="$1"
	local chat

	json_get_var device device
	json_get_var apn apn
	json_get_var username username
	json_get_var password password
	json_get_var auth auth
	json_get_var pincode pincode
	json_get_var dialnumber dialnumber
	band=$(uci_get network."$1".band)
	if [ -z "$band" ]; then
		band="1,3,5,8,20,28"
	else
		band=${band//[ ]/,}
	fi
	[ -n "$dat_device" ] && device=$dat_device
	[ -e "$device" ] || {
		proto_set_available "$interface" 0
		return 1
	}

	RESP=$(gcom -d "$device" -s /etc/gcom/getcardinfo.gcom | grep SIM7020E)
	COUNT=0
	while [ -z "$RESP" ];
	do
		sleep 1
		RESP=$(gcom -d "$device" -s /etc/gcom/getcardinfo.gcom | grep SIM7020E)
		COUNT=$((COUNT+1))
		if [ "$COUNT" -ge "10" ]; then
			logger -p daemon.info -t "nbiot" "Modem does not respond to AT commands, modem power reset"
			modem_reset
			break
		fi
	done

	COMMAND="AT+CBAND=$band" gcom -d "$device" -s /etc/gcom/runcommand.gcom &>/dev/null
	COMMAND="AT+CFUN=0" gcom -d "$device" -s /etc/gcom/runcommand.gcom &>/dev/null
	if [ -n "$username" ] && [ -n "$password" ] && [ -n "$auth" ] && [ "$auth" != "none" ]; then
		case "$auth" in
			pap) CODE=1;;
			*) CODE=2;;
		esac
		COMMAND="AT*MCGDEFCONT=\"IP\",\"$apn\",\"$username\",\"$password\",$CODE" gcom -d "$device" -s /etc/gcom/runcommand.gcom &>/dev/null
	else
		COMMAND="AT*MCGDEFCONT=\"IP\",\"$apn\"" gcom -d "$device" -s /etc/gcom/runcommand.gcom &>/dev/null
	fi
	COMMAND="AT+CFUN=1" gcom -d "$device" -s /etc/gcom/runcommand.gcom &>/dev/null

	sleep 2

	IMEI=$(gcom -d "$device" -s /etc/gcom/getimei.gcom)
	if [ -n "$IMEI" ]; then 
		IMEI=${IMEI:1}
	else
		IMEI="n/a"
	fi
	
	CCID=$(gcom -d "$device" -s /etc/gcom/getccidnb.gcom)
	if [ -n "$CCID" ]; then 
		CCID=89${CCID:0:16}
	else
		CCID="n/a"
	fi
	uci_set network "$1" imei "$IMEI"
	uci_set network "$1" ccid "$CCID"
	uci_commit network

	COMMAND="AT+CGACT=0,1" gcom -d "$device" -s /etc/gcom/runcommand.gcom &>/dev/null

	chat="/etc/chatscripts/3g.chat"
	if [ -n "$pincode" ]; then
		PINCODE="$pincode" gcom -d "$device" -s /etc/gcom/setpin.gcom || {
			proto_notify_error "$interface" PIN_FAILED
			proto_block_restart "$interface"
			return 1
		}
	fi

	if [ -z "$dialnumber" ]; then
		dialnumber="*99***1#"
	fi

	connect="${apn:+USE_APN=$apn }DIALNUMBER=$dialnumber /usr/sbin/chat -t5 -v -E -f $chat"
	ppp_generic_setup "$interface" \
		nobsdcomp \
		noauth \
		lock \
		nocrtscts \
		noipv6 \
		115200 "$device"
	return 0
}

proto_nbiot_teardown() {
	proto_kill_command "$interface"
}

[ -z "NOT_INCLUDED" ] || add_protocol nbiot
