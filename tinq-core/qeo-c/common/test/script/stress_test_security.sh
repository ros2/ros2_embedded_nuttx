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

function fail()
{
    local msg=$1
    
    echo "E - ${msg}"
    exit 1
}

function generate_certs()
{
    local realms=$1
    local i

    # 1) create root CA
    openssl req -newkey rsa:1024 -sha1 -subj "/C=BE/CN=www.qeo.org/" \
                -passout pass:secret -keyout ${WORKDIR}/rootkey.pem \
                -out ${WORKDIR}/rootreq.pem > /dev/null 2>&1 \
            || fail "failed to create root CA certificate request"
    openssl x509 -req -in ${WORKDIR}/rootreq.pem -sha1 -passin pass:secret \
                 -signkey ${WORKDIR}/rootkey.pem -out ${WORKDIR}/rootcert.pem \
                 > /dev/null 2>&1 \
            || fail "failed to create root CA certificate"
    cat ${WORKDIR}/rootcert.pem ${WORKDIR}/rootkey.pem > ${WORKDIR}/root.pem
    # 2) generate 'realms' client certificates
    for i in $(seq 1 ${realms}); do
        openssl req -newkey rsa:1024 -sha1 -subj "/C=BE/CN=www${i}.qeo.org/" \
                    -passout pass:secret -keyout ${WORKDIR}/clientkey${i}.pem \
                    -out ${WORKDIR}/clientreq${i}.pem > /dev/null 2>&1 \
                || fail "failed to create client ${i} certificate request"
        openssl x509 -req -in ${WORKDIR}/clientreq${i}.pem -sha1 \
                     -CA ${WORKDIR}/root.pem -passin pass:secret \
                     -CAkey ${WORKDIR}/root.pem -CAcreateserial \
                     -out ${WORKDIR}/clientcert${i}.pem > /dev/null 2>&1 \
                || fail "failed to create client ${i} certificate"
        openssl rsa -passin pass:secret -in ${WORKDIR}/clientkey${i}.pem \
                    -out ${WORKDIR}/clientkey${i}_noenc.pem > /dev/null 2>&1 \
                || fail "failed to remove passphrase from client ${i} key"
        cat ${WORKDIR}/rootcert.pem >> ${WORKDIR}/clientcert${i}.pem
    done
}

function start_test()
{
    local realms=$1
    local procs=$2
    local loops=$3
    local i

    for i in $(seq 1 ${realms}); do
        ${SELF_DIR}/secsystest_security -p ${procs} -l ${loops} \
                                        -c ${WORKDIR}/clientcert${i}.pem \
                                        -k ${WORKDIR}/clientkey${i}_noenc.pem &
    done
    wait
}

WORKDIR=$(mktemp -d /tmp/qeo_stress_test.XXXXXXXX)

NUM_REALM=5
NUM_PROC=25
NUM_LOOP=1000

# create certificates
generate_certs ${NUM_REALM}

# some extra vars
export TDDS_IP_INTF="eth1" #:vboxnet0"
export LD_LIBRARY_PATH=${SELF_DIR}/../lib
export MALLOC_CHECK_=2 

# start apps in their realm
start_test ${NUM_REALM} ${NUM_PROC} ${NUM_LOOP}

# clean up
rm -rf ${WORKDIR}