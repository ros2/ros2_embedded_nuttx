#!/bin/bash
# Copyright (c) 2014 - Qeo LLC
#
# The source code form of this Qeo Open Source Project component is subject
# to the terms of the Clear BSD license.
#
# You can redistribute it and/or modify it under the terms of the Clear BSD
# License (http://directory.fsf.org/wiki/License:ClearBSD). See LICENSE file
# for more details.
#
# The Qeo Open Source Project also includes third party Open Source Software.
# See LICENSE file for more details.


#VG="valgrind --num-callers=50 --leak-check=full --show-reachable=yes --track-origins=yes --log-file=vg.%p"
exit_code=0
program=$1
domain=$2
sec=$3
prefix=$4
cert=qeoCerts/first.pem
key=qeoCerts/first-key.pem
start_delay=7000
packets=4
test_timeout=180

function timeout()
{
    timer=$1
    while [ $timer -ne 0 ]
    do 
	let timer--
	printf "."
	sleep 1
    done
}

function wait_for_close()
{
        tm=0
        while [ $(ps -ef | grep -v grep | grep "${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain" | wc -l) -ne 0 ]
        do
                timeout 1
                ((tm++))                
                if [ $tm -eq $test_timeout ]
                then
                        killall -9 ${prefix}${program}
                fi
        done
}

# $1 nb of running instances
# $2 nb of packets each instance sends
function test_result()
{
    runs=0
    c=1
    for ((i=0;i<$1;i++))
    do
	if [[ ! -f ${prefix}output$c.txt ]]; then
	    echo "Fail (No output file detected)"
	    return
	fi
	output=0
	while read line
	do
	    let output++
	done < ${prefix}output$c.txt
	number=$(($2 * $1))
	if [ $output -ne $number ]
	then
	    if [ $output -le $2 ]
	    then
		echo "Fail (Wrong number of lines / only read data from itself)"
	        exit_code=1
                return 0 
	    else
		echo "Fail (Wrong number of lines / most likely read data from others as well) for output$c.txt"
                exit_code=1
            fi 
	fi
	let c++
    done
    echo "Success"
    return 1
}

# checks the log files for successfull handshakes 
function find_fail()
{
    lines=$(grep -lr "Authorisation: Authenticated" .tdds_log*)
    count=$(echo "$lines" | wc -l)
    if [ "$count" -eq 0 ]
    then
	echo "No log files were found"
    fi
    for f in .tdds_log*
    do
	lines=$(grep -r "Authorisation: Authenticated" $f)
	count=$(echo "$lines" | wc -l)
        if [[ "$count" -ne $1 ]]
	then
	    if [[ "$count" -gt $1 ]]
	    then
		echo "To much handshakes succeeded for $f , maybe some other test are running"
	    else
		echo "Some handshakes have failed for $f"
		exit_code=1
	    fi
	else
	    echo "All handshakes succeeded for $f"
	fi
    done
}

function cleanup()
{
    rm ${prefix}output*.txt
    rm .tdds_log*
    sleep 3
}

# 2 simple users #
function test1()
{
    echo "Run test 1"
    
    cleanup 
   
    ${VG} ./${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain -j ${prefix}${sec} -o $start_delay -y ${prefix}output1.txt > /dev/null &
    ${VG} ./${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain -j ${prefix}${sec} -o $start_delay -y ${prefix}output2.txt > /dev/null &

    timeout 1

    wait_for_close

    timeout 3

    test_result 2 $packets

    find_fail 2
}

# 2 simple tcp users #
function test2()
{
    echo "Run test 2"

    cleanup

    TDDS_TCP_PORT=7400 ./${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain -j ${prefix}${sec} -o $start_delay -y ${prefix}output1.txt > /dev/null &
    TDDS_TCP_SERVER=localhost:7400 TDDS_UDP_MODE=disabled ./${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain -j ${prefix}${sec} -o $start_delay -y ${prefix}output2.txt > /dev/null &

    timeout 1

    wait_for_close

    timeout 3

    test_result 2 $packets
    
    find_fail 2
}

# 2 users (1 tcp & 1 udp) + fwd #
function test3()
{
    echo "Run test 3"

    cleanup

    TDDS_FORWARD=15 TDDS_TCP_PORT=15000 ./${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain -j ${prefix}${sec} -o $start_delay -y ${prefix}output3.txt > /dev/null &
    TDDS_TCP_SERVER=localhost:15000 TDDS_UDP_MODE=disabled ./${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain -j ${prefix}${sec} -o $start_delay -y ${prefix}output1.txt > /dev/null &
    ./${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain -j ${prefix}${sec} -o $start_delay -y ${prefix}output2.txt > /dev/null &
#TDDS_IP_INTF=eth1 
    timeout 1

    wait_for_close

    timeout 3

    test_result 3 $packets

    find_fail 3
}

# 5 users #
function test4()
{
    echo "Run test 4"
    
    cleanup

    for i in {1..5}
    do
	${VG} ./${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain -j ${prefix}${sec} -o $start_delay -y ${prefix}output$i.txt > /dev/null &
    done

    timeout 1

    wait_for_close

    timeout 3

    test_result 5 $packets

    find_fail 5
}
function test5()
{
    echo "Run test 5"

    cleanup

    for i in {1..30}
    do
	${VG} ./${prefix}${program} -k ${prefix}${key} -c ${prefix}${cert} -wv -n $packets -i $domain -j ${prefix}${sec} -o $start_delay -y ${prefix}output$i.txt > /dev/null &
	sleep 0.1
    done

    timeout 1

    wait_for_close

    timeout 3

    test_result 30 $packets

    find_fail 30
}

echo "Start running tests"

#First close all previously hanging processes

killall -9 ${prefix}${program} 

test1
test2
test3
test4
test5

exit $exit_code
