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


export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vobs/do_vob1/do_store/log/HOSTLINUX/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vobs/do_vob1/do_store/dds/HOSTLINUX/
#echo $LD_LIBRARY_PATH

cd /middleware/core/servicefw/dds/project/test/systest/bin

TEST_TIMEOUT=10

echo "[US 403 - TC 2][DDS][techdds_main] script start"
logger [US 403 - TC 2][DDS][techdds_main] script start

./techdds_main -wvn20 &
PROVIDER_PID=$!
echo "PROVIDER_PID="$PROVIDER_PID
#ps aux | grep "dds -w" | grep -v grep
#sleep $TEST_TIMEOUT && echo "Timeout for provider $PROVIDER_PID" && kill $PROVIDER_PID 2> /dev/null &

sleep 3

./techdds_main -rvn20 &
CONSUMER_PID=$!
echo "CONSUMER_PID="$CONSUMER_PID
#ps aux | grep "dds -r" | grep -v grep
#sleep $TEST_TIMEOUT && echo "Timeout for consumer $CONSUMER_PID" && kill $CONSUMER_PID 2> /dev/null &

#wait for processes to stop
wait 2> /dev/null

echo "[US 403 - TC 2][DDS][techdds_main] script stop"
logger [US 403 - TC 2][DDS][techdds_main] script stop
exit 0
