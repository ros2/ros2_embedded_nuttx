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

#
#  Automatic Qeo-c build/package script for iPhoneOS (armv7, armv7s, arm64) and 
#  iPhoneSimulator (i386 and x86_64)

# Abort script on errors
set -e
#set -x
#env
unset INSTALL_DIR
DEBUG=0


usage()
{
    echo "This script builds libqeo.a for all supported architectures."
    echo "Options:"
    echo "  -s : Strips libqeo.a"
    echo "  -d : Build with DEBUG=1"
}

while getopts “hsd” OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
             ;;
         s)
             STRIP=1
             echo "STRIP=1"
             ;;
         d)
             DEBUG=1
             echo "DEBUG=1"
             ;;
         ?)
             usage
             exit
             ;;
     esac
done

# Sets the framework name and version.
FMK_NAME=qeo-c-core
FMK_VERSION=A
ARCHITECTURES="IOS_ARMV7_CLANG IOS_ARMV7S_CLANG IOS_ARM64_CLANG IOS_SIM32_CLANG IOS_SIM64_CLANG" 

# Setup your paths
CURRENTPATH=`pwd`
WRK_DIR=${CURRENTPATH}/tmp
FRAMEWORK_DIR=${CURRENTPATH}/Products/${FMK_NAME}.framework

# Remove any old build or product files
rm -rf ${WRK_DIR}
rm -rf ${FRAMEWORK_DIR}

# Setup build directories
# Build Qeo binaries
for ARCH in ${ARCHITECTURES}
do
    mkdir -p "${WRK_DIR}/${ARCH}"
    make -f Makefile_component.ios E=${ARCH} DEBUG=${DEBUG} install DESTDIR=${WRK_DIR}/${ARCH} &
done
wait

#put everything in a big archive
for ARCH in ${ARCHITECTURES}
do
    cd ${WRK_DIR}/${ARCH}/usr/local/lib
    for ARCHIVE in *.a
    do
        ar x ${ARCHIVE}
        if [[ $STRIP -eq 1 ]]; then
            echo "stripping :" *.o
            strip -x *.o
        fi
        ar cqs libqeo.a *.o
        rm *.o ${ARCHIVE}
    done
    cd -
    LIPO_INPUT="${LIPO_INPUT}${WRK_DIR}/${ARCH}/usr/local/lib/libqeo.a "
done

# Creates and renews the final Products folder.
mkdir -p "${FRAMEWORK_DIR}"
mkdir -p "${FRAMEWORK_DIR}/Versions"
mkdir -p "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}"
mkdir -p "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Resources"
mkdir -p "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers"
mkdir -p "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/qeo-tsm-to-dynamic"

# Creates the internal links.
# Use relative paths here, otherwise will not work when the folder is copied/moved.
ln -s "${FMK_VERSION}" "${FRAMEWORK_DIR}/Versions/Current"
ln -s "Versions/Current/Headers" "${FRAMEWORK_DIR}/Headers"
ln -s "Versions/Current/Resources" "${FRAMEWORK_DIR}/Resources"
ln -s "Versions/Current/${FMK_NAME}" "${FRAMEWORK_DIR}/${FMK_NAME}"

# Copy all public header files to the final product folder.
cd ../..
NEW_PATH=`pwd`
mkdir -p "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/dds"
cp "${NEW_PATH}/dds/api/headers/dds/dds_dcps.h" "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/dds/dds_dcps.h"
cp "${NEW_PATH}/dds/api/headers/dds/dds_error.h" "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/dds/dds_error.h"
cp "${NEW_PATH}/dds/api/headers/dds/dds_seq.h" "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/dds/dds_seq.h"
cp "${NEW_PATH}/dds/api/headers/dds/dds_tsm.h" "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/dds/dds_tsm.h"
cp "${NEW_PATH}/dds/api/headers/dds/dds_types.h" "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/dds/dds_types.h"
cp "${NEW_PATH}/dds/api/headers/dds/dds_aux.h" "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/dds/dds_aux.h"
cp -R ${NEW_PATH}/qeo-c/qeo-c-core/api/headers/* "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/"
cp -R ${NEW_PATH}/qeo-c/qeo-c/api/headers/* "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/"
cp -R ${NEW_PATH}/qeo-c/qeo-c-util/api/headers/* "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/"
cp -R ${NEW_PATH}/qeo-objc/qeo-tsm-to-dynamic/api/headers/* "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/qeo-tsm-to-dynamic" 

# Fix include path
gsed -i "s/\"dds/\"${FMK_NAME}\/dds/" "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/Headers/dds/"dds_*h 

# Merge targets into one binary
lipo -create ${LIPO_INPUT} -output "${FRAMEWORK_DIR}/Versions/${FMK_VERSION}/${FMK_NAME}"

# Cleanup the working dir
rm -r "${WRK_DIR}"

# Tell the log we are done here !
echo "${FMK_NAME} has been built successfully !"
