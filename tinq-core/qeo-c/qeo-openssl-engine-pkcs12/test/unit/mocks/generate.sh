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

# Mocking of qeo_platform
QEO_PLATFORM_SRC_DIR=../../../../qeo-c-util/api/headers/qeo
QEO_PLATFORM_SRC_FILES2MOCK="platform"
for f in ${QEO_PLATFORM_SRC_FILES2MOCK}; do
	mock ${QEO_PLATFORM_SRC_DIR} $f
    sed -i '5s/^/#include "unity.h"\n/' Mock$f.h
    sed -i "s/#include \"$f.h\"/#include \"qeo\/$f.h\"/" Mock$f.h
    sed -i '/#include "unity\.h"/s/^/#define NO_TEST_ASSERT_EQUAL_MEMORY_MESSAGE\n/' Mock$f.c
done

