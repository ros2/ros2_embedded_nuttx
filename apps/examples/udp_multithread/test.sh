#!/bin/bash

for i in $(seq 1 1 20)
do
	sudo sendip -p ipv4 -is 192.168.0.2 -p udp -us 5070 -ud 6666 -d "Hello 1" -v 192.168.0.3
	sudo sendip -p ipv4 -is 192.168.0.2 -p udp -us 5071 -ud 6667 -d "Hello 2" -v 192.168.0.3
	sudo sendip -p ipv4 -is 192.168.0.2 -p udp -us 5072 -ud 6668 -d "Hello 3" -v 192.168.0.3
	sudo sendip -p ipv4 -is 192.168.0.2 -p udp -us 5073 -ud 6669 -d "Hello 4" -v 192.168.0.3
done 
