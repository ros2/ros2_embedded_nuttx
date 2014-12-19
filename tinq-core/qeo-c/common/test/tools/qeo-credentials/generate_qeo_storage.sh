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


SELF_DIR=$(cd $(dirname $0) > /dev/null; pwd)

function syntax()
{
    echo "Usage: $(basename $0) <qeostoragedir> <truststore> <realmid> <deviceid> <userid> [<policyrule>]"
    echo "Use this script to generate a Qeo home dir that can be used without a need to go to the server."
    echo "The truststore location must point to a directory where all the certificates are managed."
    echo "Certificates already created inside that location will be reused when applicable."
    exit 1
}   

if [ $# -lt 5 ]; then
    syntax
fi

QEO_DIR=$1
TRUSTSTORE_DIR=$2
REALM_ID=$3
DEVICE_ID=$4
USER_ID=$5
POLICY_RULE=$6

# Create a truststore or use an existing trustore that contains the Qeo PKI stuff to use
${SELF_DIR}/generate_truststore.sh "${TRUSTSTORE_DIR}" "${REALM_ID}" "${DEVICE_ID}" "${USER_ID}" "${POLICY_RULE}"
P12_FILE="${TRUSTSTORE_DIR}/${REALM_ID}_${DEVICE_ID}_${USER_ID}/truststore.p12"
POLICY_FILE="${TRUSTSTORE_DIR}/${REALM_ID}_policy/${REALM_ID}_policy.mime"

# Create the Qeo storage directory 
mkdir ${QEO_DIR} 2>/dev/null
QEO_DIR=$(cd ${QEO_DIR} > /dev/null; pwd)
# copy policy file and pkcs12 file containing certificates from the truststore location to the qeo storage dir
cp -v ${POLICY_FILE} ${QEO_DIR}
cp -v ${P12_FILE} "${QEO_DIR}/truststore.p12"

# Create random uuid
cp -v "/proc/sys/kernel/random/uuid" "${QEO_DIR}/uuid"
chmod 666 "${QEO_DIR}/uuid" 
# Make sure we use a file url that points to a root resource that we created.
# The root rescource only supports fetching forwarders from another file url. 
SERVER_URL="file://${QEO_DIR}/root_resource.json"
FWD_URL="file://${QEO_DIR}/forwarders.json"
cp -v ${SELF_DIR}/root_resource.json "${QEO_DIR}"
# Fill in the correct forwarder url inside the root resource
FWD_URL_ESC="$(echo ${FWD_URL} | sed 's/\//\\\//g')"
sed -i "s/xxxxxx/${FWD_URL_ESC}/g" ${QEO_DIR}/root_resource.json
cp -v ${SELF_DIR}/forwarders.json "${QEO_DIR}"
# Use the correct url in the qeo storage dir
echo "${SERVER_URL}" > "${QEO_DIR}/url"

echo "Created qeo storage dir at ${QEO_DIR} for ${REALM_ID} ${DEVICE_ID} ${USER_ID}" 
