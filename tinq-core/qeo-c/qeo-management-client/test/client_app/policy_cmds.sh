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

if [ $# -ne 5 ]
then
echo 'Usage: $0 <url> <location_for_p12_file> <password_for_PKCS12> <path_and_name_for_policy_file> <verify(1 or 0)>'  
echo 'Will retrieve policy file from url based on device certificate and verify the smime syntax and certificates if asked for'  
echo 'Exit codes are:'
echo '0: success'
echo '1: General failure while enrolling device'
echo '2: Invalid arguments'
echo '3: Memory problems'
echo '4: Invalid otp'
echo '5: Could not connect'
echo '6: SSL setup problem'
echo '8: The url for this service is not inside the root resource.'
echo '255: Other issues'
exit 255
else
    DIRECTORY=`dirname $0`
	TMPDIR=$(mktemp -d /tmp/policyXXXXXX)
    
    mkdir -p $(dirname $2)
    mkdir -p $(dirname $4)
    set -e
    LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app policy -u ${1} -c ${2} -p ${3} -f ${4}

	if [ "$5" = "1" ]
	then
		#Verify the smime signature
		openssl smime -in ${4} -verify -noverify
        echo "validating smime signature $?"
		#Verify the policy signing certificate
		openssl pkcs12 -in ${2} -password "pass:${3}" -cacerts -passout "pass:${3}" -nokeys > ${TMPDIR}/ca.pem
		openssl smime -in ${4} -pk7out | openssl pkcs7 -print_certs > ${TMPDIR}/policy.pem
		openssl verify -purpose any -CAfile ${TMPDIR}/ca.pem ${TMPDIR}/policy.pem
		echo "validating smime policy certificate $?"
	fi
    rm -rf $TMPDIR
fi
