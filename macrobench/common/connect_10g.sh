#!/bin/bash

# IP ADDR should be not used by system

# See document https://docs.openvswitch.org/en/latest/faq/issues/
# For more details about difference between tap device and int device in openvswitch.

source ../server_conf

if [ "$1" = "server" ]; then
	sudo ip addr add $NET_IP_SERVER/24 dev $NET_INT
	sudo ip link set $NET_INT up
elif [ "$1" = "client" ]; then
	sudo ip addr add $NET_IP_CLIENT/24 dev $NET_INT
	sudo ip link set $NET_INT up
elif [ "$1" = "flush" ]; then
	sudo ip addr flush dev $NET_INT
	sudo ifconfig $NET_INT 0.0.0.0 down
else
	echo "Usage: server, client or flush?"
fi;
