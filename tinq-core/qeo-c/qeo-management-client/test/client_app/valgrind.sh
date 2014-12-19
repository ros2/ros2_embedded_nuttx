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

# Run valgrind analysis of sunny day scenario enrolling device and retrieving policy file.

if [ $# -ne 4 ]
then
echo 'Usage: $0 <url> <otc1> <otc2> <valgrind tool>'  
echo 'Will retrieve 2 device certificates from url based on otcs'
echo 'Using these device certificates the policy is checked and retrieved'
echo 'Also these devices are registered as forwarders and a list of forwarders is retrieved'
echo 'Possible valgrind tools are: massif, drd, helgrind, memcheck'  
exit 1
fi

url=$1
otc1=$2
otc2=$3
valgrindtool=$4
DIRECTORY=`dirname $0`
TMPDIR=$(mktemp -d /tmp/qmcXXXXXX)
echo "tmpdir is ${TMPDIR}"

cmd=''

if [ "massif" = $valgrindtool ]; then
    cmd='valgrind --tool=massif --time-unit=ms'
else
    cmd="valgrind --trace-children=yes --num-callers=50"
    if [ "memcheck" = $valgrindtool ]; then
        cmd="${cmd} --leak-check=full --show-reachable=yes --track-origins=yes --suppressions=${DIRECTORY}/valgrind.supp"
    elif [ "helgrind" = $valgrindtool ]; then
        cmd="${cmd} --tool=helgrind --read-var-info=yes"
    elif [ "drd" = $valgrindtool ]; then
        cmd="${cmd} --tool=drd --read-var-info=yes"
    fi
fi
echo ${cmd} 

set -e
LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_device1.log ${DIRECTORY}/qeo-mgmt-client-app device -u ${url} -d "valgrinddevice1" -o ${otc1} -c $TMPDIR/devicecert1.file -p bla
LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_policy1.log ${DIRECTORY}/qeo-mgmt-client-app policy -u ${url} -c $TMPDIR/devicecert1.file -p bla -f $TMPDIR/policy1.file -m 2
LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_device2.log ${DIRECTORY}/qeo-mgmt-client-app device -u ${url} -d "valgrinddevice2" -o ${otc2} -c $TMPDIR/devicecert2.file -p bla
LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_policy2.log ${DIRECTORY}/qeo-mgmt-client-app policy -u ${url} -c $TMPDIR/devicecert2.file -p bla -f $TMPDIR/policy2.file -m 2

realmid1=$(cat $TMPDIR/policy1.file | grep "\[rid" | cut -d ":" -f 2 | cut -d "]" -f 1) 
seqnr1=$(cat $TMPDIR/policy1.file | grep "seqnr=" | cut -d = -f 2) 
realmid2=$(cat $TMPDIR/policy2.file | grep "\[rid" | cut -d ":" -f 2 | cut -d "]" -f 1) 
seqnr2=$(cat $TMPDIR/policy2.file | grep "seqnr=" | cut -d = -f 2) 
LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_policycheck1.log ${DIRECTORY}/qeo-mgmt-client-app check_policy -u ${url} -r ${realmid1} -s ${seqnr1} -m 2
LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_policycheck2.log ${DIRECTORY}/qeo-mgmt-client-app check_policy -u ${url} -r ${realmid2} -s ${seqnr2} -m 2

LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_registerfwd1.log ${DIRECTORY}/qeo-mgmt-client-app register_fwd -u ${url} -c $TMPDIR/devicecert1.file -p bla -f "${DIRECTORY}/testdata/forwarder_multiple_locators"
LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_listfwds1.log ${DIRECTORY}/qeo-mgmt-client-app list_fwd -u ${url} -c $TMPDIR/devicecert1.file -p bla -f $TMPDIR/forwarders1.file -m 2
LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_registerfwd2.log ${DIRECTORY}/qeo-mgmt-client-app register_fwd -u ${url} -c $TMPDIR/devicecert2.file -p bla -f "${DIRECTORY}/testdata/forwarder_single_locator"
LD_LIBRARY_PATH="${DIRECTORY}/../lib" $cmd --log-file=${TMPDIR}/valgrind_listfwds2.log ${DIRECTORY}/qeo-mgmt-client-app list_fwd -u ${url} -c $TMPDIR/devicecert2.file -p bla -f $TMPDIR/forwarders2.file -m 2
#rm -rf $TMPDIR
echo "Successfully run valgrind analysis"
ls -l ${TMPDIR}/valgrind*.log
