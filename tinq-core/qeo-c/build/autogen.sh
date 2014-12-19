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
# The MediaGEN build environment relies on small makefiles, called .env files,
# that describe how we need to build for a certain target architecture. These
# .env files are stored in the build directory in the subdirectory
# supported_targets. 
#
# This shell script can be used to create a configure script that can in turn
# be used to create an .env file for a new architecture. This .env file will be
# called configure.env and will be placed in the supported_targets directory. 
# 
# Furthermore, this shell script will also generate a small wrapper Makefile
# around Makefile_component, that behaves more like a standard automake
# makefile (i.e. install does a devel_install), a distclean target is added.

# Canonicalize the path to this script 
EXE=`readlink -f $0`
# Store the working directory
CALLING_DIR=`pwd`

# Generate the wrapper Makefile
test -e Makefile_component && test ! -e Makefile && echo "E=configure@EMULATE_AUTOTOOLS=1@include Makefile_component@@distclean:@%rm -rf Makefile build/aclocal.m4 build/autom4te.cache build/config.guess build/config.sub build/configure build/install-sh build/install.sh build/ltmain.sh build/supported_targets/configure.env config.log config.status libtool output" | tr "@%" "\n\t" > Makefile

# Go to the directory that contains this script
cd `dirname $EXE`

# Make sure autoconf will run:

# 1. Make sure we have install-sh. autoconf requires it

touch install-sh

# 2. If libtoolize supports -i (install missing files) we need to add it
if libtoolize --help | grep -- -i
then
	EXTRA=-i
else
	EXTRA=
fi

# 3. Call libtoolize, and filter out messages that we should add the contents
#    of some macro's....
libtoolize -c --force $EXTRA  | grep -v "You should add the contents of"

# 4. Run aclocal
aclocal 

# 5. Run autoconf
autoconf 

# Return to working dir 
cd $CALLING_DIR
