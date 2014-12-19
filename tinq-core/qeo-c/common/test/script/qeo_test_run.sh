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

. ${SELF_DIR}/addon-valgrind.source
. ${SELF_DIR}/addon-qeocreds.source

VG_ARGS="/tmp qeo"
VG=$(valgrind_cmd ${VG_ARGS} ${SELF_DIR}/valgrind.supp)

# Avoid interference from other apps by using non-default domain
if [ -z "${QEO_DDS_DOMAIN_ID_CLOSED}" ]; then
    #get number from hostname
    QEO_DDS_DOMAIN_ID_CLOSED=$(hostname | sed 's/cplx//;s/\..*//' | sed 's/edgmwbuild//;s/\..*//')
    #check if it's a number
    echo ${QEO_DDS_DOMAIN_ID_CLOSED} | grep -E "[0-9]+" >/dev/null 2>&1
    if [ $? != 0 ]; then
        #default value
        QEO_DDS_DOMAIN_ID_CLOSED=37
    fi
    QEO_DDS_DOMAIN_ID_CLOSED=$(( ${QEO_DDS_DOMAIN_ID_CLOSED} % 200 + 2 )) #make sure never to use domain 0 or 1
    export QEO_DDS_DOMAIN_ID_CLOSED
fi
if [ -z "${QEO_DDS_DOMAIN_ID_OPEN}" ]; then
    QEO_DDS_DOMAIN_ID_OPEN=$(( ${QEO_DDS_DOMAIN_ID_CLOSED} + 1 ))
    export QEO_DDS_DOMAIN_ID_OPEN
fi

export QEO_FWD_DISABLE_LOCATION_SERVICE=1

# run unit tests
echo "=== UNIT TESTS ==="

# start out with no credentials for unit tests
export QEO_STORAGE_DIR=${SELF_DIR}/../tmp/home.qeo/
rm -rf ${QEO_STORAGE_DIR} > /dev/null 2>&1
mkdir -p ${QEO_STORAGE_DIR}

if [[ "${1}" == "create_qeo_home_for_junit" ]]
then
    #Create qeo credentials
    create_home_qeo
    set_home_qeo "qeo"
fi

# remove lock file
rm -f /tmp/.qeo*.lock 2>/dev/null

if [ -d ${SELF_DIR}/../lib/unittests ]; then
    clean_valgrind ${VG_ARGS}
    CK_DEFAULT_TIMEOUT=20 ${VG} ${SELF_DIR}/unittest --suitedir ${SELF_DIR}/../lib/unittests --all --nml || exit 1
    check_valgrind ${VG_ARGS} || exit 1
fi

if [ -d ${SELF_DIR}/../lib/unittests_novg ]; then
    # don't abort on Valgrind error
    CK_DEFAULT_TIMEOUT=20 ${VG} ${SELF_DIR}/unittest --suitedir ${SELF_DIR}/../lib/unittests_novg --all || exit 1
fi

# run system tests
echo "=== SYSTEM TESTS ==="
if [[ "${1}" != "create_qeo_home_for_junit" ]]
then
    #Create qeo credentials
    create_home_qeo
fi
clean_valgrind ${VG_ARGS}
for test in $(find ${SELF_DIR}/systest/ -type f 2> /dev/null); do
	echo "Running $(basename ${test})"
	set_home_qeo $(basename ${test})
	${VG} ${test} || exit 1
	echo " -> result = $?"
done
check_valgrind ${VG_ARGS} || exit 1

# run system tests without Valgrind abort
echo "=== SECURITY SYSTEM TESTS ==="

clean_valgrind ${VG_ARGS}
for test in $(find ${SELF_DIR}/systest_novg/ -type f 2> /dev/null); do
	echo "Running $(basename ${test})"
	${VG} ${test} || exit 1
	echo " -> result = $?"
done
# don't abort on Valgrind error
check_valgrind ${VG_ARGS}

echo "=== FAILING SYSTEM TESTS ==="

clean_valgrind ${VG_ARGS}
for test in $(find ${SELF_DIR}/systest_fail/ -type f 2> /dev/null); do
	echo "Running $(basename ${test})"
	set_home_qeo $(basename ${test})
	${VG} ${test}
	echo " -> result = $?"
done
check_valgrind ${VG_ARGS}

echo "=== DISABLED SYSTEM TESTS ==="

for test in $(find ${SELF_DIR}/systest_disabled/ -type f 2> /dev/null); do
	echo "SKIPPING: $(basename ${test})"
done

echo "=== DONE ==="

exit 0
