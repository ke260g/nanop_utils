#!/bin/bash
IFACE_ARRAY=( "wlan0" "eth0" )

for IFACE in ${IFACE_ARRAY[@]} ; do
IP=`ip addr show dev ${IFACE}| sed -n "/inet[^6]/{s/inet//; s/\/.*$//; p}"`
# echo -e "${IFACE}\c";\
echo -e "\e[32m${IFACE} \e[36;1m${IP}\e[0m"
done
