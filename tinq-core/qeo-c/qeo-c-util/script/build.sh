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

#Platform integrators: adapt this script to your own needs

set -e
SELF_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $SELF_DIR
platform_PATH_SRC=../src/qeo/
platform_PATH_INC=../include/
ARCH=`uname -m`
if [[ $ARCH == "arm"* ]]; then
    EXTRA_CFLAGS=""
else
    EXTRA_CFLAGS="-m32"
fi
gcc $EXTRA_CFLAGS -Wall -Werror -O0 -g -shared -I${platform_PATH_INC} -fPIC -o libqeoutil.so  ${platform_PATH_SRC}/*.c

echo "libqeoutil.so was successfully built !"
cd -

