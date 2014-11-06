#!/bin/bash

for i in $(seq 1 1 200)
do
	ALEATORIO=$(shuf -i 6666-6669 -n 1)
	echo $ALEATORIO
	sudo sendip -p ipv4 -is 192.168.0.2 -p udp -us 5070 -ud $ALEATORIO -d "Hello $ALEATORIO " -v 192.168.0.3
	sudo sendip -p ipv4 -is 192.168.0.2 -p udp -us 5070 -ud 6666 -d "Hello multicast" -v 239.255.0.1
done 
