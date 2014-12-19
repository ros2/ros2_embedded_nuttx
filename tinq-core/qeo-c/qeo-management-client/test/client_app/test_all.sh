#!/bin/sh
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

# Test all functionality of the client library against a server.

if [ $# -ne 3 ]
then
echo 'Usage: $0 <url> <otc1> <otc2>'  
echo 'Will retrieve 2 device certificates from url based on otcs'
echo 'Using these device certificates the policy is checked and retrieved'
echo 'Also these devices are registered as forwarders and a list of forwarders is retrieved'
exit 1
fi

url=$1
otc1=$2
otc2=$3
DIRECTORY=`dirname $0`
TMPDIR=$(mktemp -d /tmp/qmcXXXXXX)
echo "tmpdir is ${TMPDIR}"

set -e
echo "Registering device_1 based on otc:${otc1}"
LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app device -u ${url} -d "device_1" -o ${otc1} -c $TMPDIR/devicecert1.file -p bla > $TMPDIR/enrolldevice1.log 2>&1
echo "Retrieving policy for device 1"
LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app policy -u ${url} -c $TMPDIR/devicecert1.file -p bla -f $TMPDIR/policy1.file > $TMPDIR/pullpolicy1.log 2>&1
cat "$TMPDIR/policy1.file"
echo ""

echo "Registering device_2 based on otc:${otc2}"
LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app device -u ${url} -d "device_2" -o ${otc2} -c $TMPDIR/devicecert2.file -p bla > $TMPDIR/enrolldevice2.log 2>&1
echo "Retrieving policy for device 2"
LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app policy -u ${url} -c $TMPDIR/devicecert2.file -p bla -f $TMPDIR/policy2.file > $TMPDIR/pullpolicy2.log 2>&1
cat "$TMPDIR/policy2.file"
echo ""

realmid1=$(cat $TMPDIR/policy1.file | grep "\[rid" | cut -d ":" -f 2 | cut -d "]" -f 1) 
seqnr1=$(cat $TMPDIR/policy1.file | grep "seqnr=" | cut -d = -f 2) 
realmid2=$(cat $TMPDIR/policy2.file | grep "\[rid" | cut -d ":" -f 2 | cut -d "]" -f 1) 
seqnr2=$(cat $TMPDIR/policy2.file | grep "seqnr=" | cut -d = -f 2) 
echo ""
echo "Checking policy based on new sequence number ${seqnr2} and realm ${realmid2} that is up to date"
LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app check_policy -u ${url} -r ${realmid2} -s ${seqnr2} > $TMPDIR/checkpolicy2.log 2>&1

echo "Registering forwarder_1 based on following json file"
cat "${DIRECTORY}/testdata/forwarder_multiple_locators"
echo ""
LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app register_fwd -u ${url} -c $TMPDIR/devicecert1.file -p bla -f "${DIRECTORY}/testdata/forwarder_multiple_locators" > $TMPDIR/registerfwd1.log 2>&1
echo "Retrieving list of forwarders."
LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app list_fwd -u ${url} -c $TMPDIR/devicecert1.file -p bla -f $TMPDIR/forwarders1.file > $TMPDIR/listfwd1.log 2>&1
cat "${TMPDIR}/forwarders1.file"
echo "Registering forwarder_2 based on following json file"
cat "${DIRECTORY}/testdata/forwarder_single_locator"
LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app register_fwd -u ${url} -c $TMPDIR/devicecert2.file -p bla -f "${DIRECTORY}/testdata/forwarder_single_locator" > $TMPDIR/registerfwd2.log 2>&1
echo "Retrieving list of forwarders."
LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app list_fwd -u ${url} -c $TMPDIR/devicecert2.file -p bla -f $TMPDIR/forwarders2.file > $TMPDIR/listfwd2.log 2>&1
cat "${TMPDIR}/forwarders2.file"
echo ""
#rm -rf $TMPDIR
echo "You can find all the outputs inside ${TMPDIR}"
ls -l ${TMPDIR}/*.log
