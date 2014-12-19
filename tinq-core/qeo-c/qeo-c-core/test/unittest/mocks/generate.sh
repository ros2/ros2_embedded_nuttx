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


CMOCK_OPTS=cmock_opts.yml
CMOCK_PATH=../../../../../qeo-c-import/cmock/src/lib/cmock.rb

if [ ! -e ${CMOCK_OPTS} ]; then
    echo "This script should be run from the 'mocks' directory in which it"
    echo "is located."
    exit 1
fi

function mock_base() {
	local optsfile=$1
	local dir=$2
	local filename=$3
    local fullname=${dir}/${filename}.h

    echo "Mocking $1..."
    ruby ${CMOCK_PATH} -o${optsfile} ${fullname}
}

function mock() {
    mock_base cmock_opts.yml $*
}

# Qeo src mocking

QEO_SRC_DIR=../../../src
QEO_SRC_FILES2MOCK="core typesupport samplesupport crc32 list entity_store user_data forwarder"
for f in ${QEO_SRC_FILES2MOCK}; do
	mock ${QEO_SRC_DIR} $f
    sed -i '5s/^/#include "unity.h"\n/' Mock$f.h
    sed -i '/#include "unity\.h"/s/^/#define NO_TEST_ASSERT_EQUAL_MEMORY_MESSAGE\n/' Mock$f.c
done

# DDS mocking

DDS_DIR=../../../../../dds/api/headers
DDS_FILES2MOCK="dds_dcps dds_aux"
for f in ${DDS_FILES2MOCK}; do
    H=$f.h
    # First run the preprocessor over the input file (force include handling)
    # Also strip out 'DDS_alloc_fcts_set' because the cmock parser can't handle it
    cpp -I . -I ${DDS_DIR} ${DDS_DIR}/dds/$H | awk '/DDS_alloc_fcts_set/,/;/{next}' > $H
    # Now mock it
    mock . $f
    # Some fixes
    sed -i 's/#include "dds/#include "dds\/dds/' Mock$H
    sed -i '5s/^/#include "unity.h"\n/' Mock$H
    # And clean up afterwards
    rm $H
done

# Mocking of the qeo-management-client API (is in another repo)
QEO_MGMT_SRC_DIR=../../../../qeo-management-client/api/headers/qeo
QEO_MGMT_SRC_FILES2MOCK="mgmt_client mgmt_client_forwarder mgmt_cert_parser"
for f in ${QEO_MGMT_SRC_FILES2MOCK}; do
	mock ${QEO_MGMT_SRC_DIR} $f
    sed -i '5s/^/#include "unity.h"\n/' Mock$f.h
    sed -i "s/#include \"$f.h\"/#include \"qeo\/$f.h\"/" Mock$f.h
    sed -i '/#include "unity\.h"/s/^/#define NO_TEST_ASSERT_EQUAL_MEMORY_MESSAGE\n/' Mock$f.c
done


# Mocking of qeo_platform_security
QEO_PLATFORM_SEC_SRC_DIR=../../../../qeo-c-util/api/headers/qeo
QEO_PLATFORM_SEC_SRC_FILES2MOCK="platform_security"
for f in ${QEO_PLATFORM_SEC_SRC_FILES2MOCK}; do
	mock ${QEO_PLATFORM_SEC_SRC_DIR} $f
    sed -i '5s/^/#include "unity.h"\n/' Mock$f.h
    sed -i "s/#include \"$f.h\"/#include \"qeo\/$f.h\"/" Mock$f.h
    sed -i '/#include "unity\.h"/s/^/#define NO_TEST_ASSERT_EQUAL_MEMORY_MESSAGE\n/' Mock$f.c
done

# Mocking of qeocore/security
QEO_PLATFORM_SEC_POL_SRC_DIR=../../../../qeo-c-core/src/security/
QEO_PLATFORM_SEC_POL_SRC_FILES2MOCK="policy security security_util remote_registration"
for f in ${QEO_PLATFORM_SEC_POL_SRC_FILES2MOCK}; do
	mock ${QEO_PLATFORM_SEC_POL_SRC_DIR} $f
    sed -i '5s/^/#include "unity.h"\n/' Mock$f.h
    sed -i '/#include "unity\.h"/s/^/#define NO_TEST_ASSERT_EQUAL_MEMORY_MESSAGE\n/' Mock$f.c
done

# Mocking of qeocore/deviceinfo
QEO_PLATFORM_SEC_POL_SRC_DIR=../../../../qeo-c-core/src/deviceinfo/
QEO_PLATFORM_SEC_POL_SRC_FILES2MOCK="deviceinfo_writer"
for f in ${QEO_PLATFORM_SEC_POL_SRC_FILES2MOCK}; do
	mock ${QEO_PLATFORM_SEC_POL_SRC_DIR} $f
    sed -i '5s/^/#include "unity.h"\n/' Mock$f.h
    sed -i '/#include "unity\.h"/s/^/#define NO_TEST_ASSERT_EQUAL_MEMORY_MESSAGE\n/' Mock$f.c
done

# Mocking of qeocore/api.h
QEO_CORE_API_DIR=../../../../qeo-c-core/api/headers/qeocore
QEO_CORE_API_FILES2MOCK="api dyntype factory"
for f in ${QEO_CORE_API_FILES2MOCK}; do
    cat ${QEO_CORE_API_DIR}/$f.h | sed '/DDS_SEQUENCE(qeocore_enum_constant_t, qeocore_enum_constants_t)/d' > $f.h
    # Now mock it
    mock . $f
    # Some fixes
    sed -i '5s/^/#include "unity.h"\n/' Mock$f.h
    sed -i "s/#include \"$f.h\"/#include \"qeocore\/$f.h\"/" Mock$f.h
    sed -i '/#include "unity\.h"/s/^/#define NO_TEST_ASSERT_EQUAL_MEMORY_MESSAGE\n/' Mock$f.c
    # And clean up afterwards
    rm $f.h
done
