#!/bin/bash

for i in $(seq 1 1 20)
do
	echo -n "foo 1" | nc -u -w1 192.168.0.3 6666
	echo -n "foo 2" | nc -u -w1 192.168.0.3 6667
	echo -n "foo 3" | nc -u -w1 192.168.0.3 6668
	echo -n "foo 4" | nc -u -w1 192.168.0.3 6669
done 
