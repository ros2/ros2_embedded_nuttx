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

# Script that is used to generate json test data to use inside unit tests based on a bunch of json files.
# Currently only the data is generated, not the functions to retrieve them.
 
echo "$0 <target-file>"
scriptdirname=$(dirname $0)
cat ${scriptdirname}/json_messages_begin.c > $1
find ${scriptdirname} -mindepth 1 -type d | while read dir;  
do
dirname=$(basename $dir)
echo "char* ${dirname}[]={" >> $1
ls $dir | while read file; do sed -e 's/\\/\\\\/g;s/"/\\"/g;s/  /\\t/g;s/^/"/;s/$/\\n"/' $dir/$file >> $1; echo "," >> $1; done
echo "NULL};" >> $1

done
cat ${scriptdirname}/json_messages_end.c >> $1
