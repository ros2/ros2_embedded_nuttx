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
echo 'Usage: $0 <register/get> <url> <location_for_p12_file> <password_for_PKCS12> <path_to_json_file>'  
echo 'Will register forwarder based on json file or retrieve forwarder list and store it as json file.'  
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
    
    mkdir -p $(dirname $5)
    set -e
	action="list_fwd"
	if [ "$1" = "register" ]
	then
		action="register_fwd"
	fi
    LD_LIBRARY_PATH="${DIRECTORY}/../lib" ${DIRECTORY}/qeo-mgmt-client-app ${action} -u ${2} -c ${3} -p ${4} -f ${5}
fi
