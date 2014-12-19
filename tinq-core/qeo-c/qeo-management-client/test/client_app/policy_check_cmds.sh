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

# Commands to retrieve a certificate request

if [ $# -ne 3 ] && [ $# -ne 2 ]
then
echo 'Usage: $0 <url> <realmid> <sequence_nr>'
echo 'or'
echo 'Usage: $0 <url> <policyfile>'
echo 'Will check whether the policy file (or realmid and sequencenr) is still valid within the realm.'  
echo 'Exit codes are:'
echo '0: success, the information is still valid'
echo '66: Successfully checked the status but it is no longer valid'
echo '1: General failure while enrolling device'
echo '2: Invalid arguments'
echo '3: Memory problems'
echo '4: Invalid otp'
echo '5: Could not connect'
echo '6: SSL setup problem'
echo '7: Problem with the realmid or sequence nr'
echo '8: The url for this service is not inside the root resource'
echo '255: Other issues'
exit 255
else
    DIRECTORY=`dirname $0`
    
    set -e
	realmid=0
	seqnr=0
    if [ "$#" = "2" ]
	then
		realmid=$(cat ${2} | grep "\[rid" | cut -d ":" -f 2 | cut -d "]" -f 1) 
		seqnr=$(cat ${2} | grep "seqnr=" | cut -d = -f 2) 
	else
		realmid=${2}
		seqnr=${3} 
	fi
    LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app check_policy -u ${1} -r ${realmid} -s ${seqnr}


#rm -rf $TMPDIR
fi
