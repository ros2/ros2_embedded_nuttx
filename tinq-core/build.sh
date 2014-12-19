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
    echo "Usage: $(basename $0) <dir where to put all generated binaries>"
    exit 1
}   

if [ $# -ne 1 ]; then
    syntax
fi
mkdir -p $1 2>/dev/null
INSTALL_DIR=$(cd $1 > /dev/null; pwd)
set -e

cd "${SELF_DIR}/qeo-c/qeo-native"
make -f Makefile_component DESTDIR=$INSTALL_DIR recursive_devel_install
cd "${SELF_DIR}/qeo-c/qeo-forwarder"
make -f Makefile_component DESTDIR=$INSTALL_DIR recursive_devel_install
echo "You can find the resulting binaries at ${INSTALL_DIR}"
