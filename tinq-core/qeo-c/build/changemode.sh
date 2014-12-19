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


set -e 
if [ -n "$MAKE_MANIFEST" -a ! -e META-INF/MANIFEST.MF ]
then
	mkdir -p META-INF
	echo Manifest-Version: 1.0 > META-INF/MANIFEST.MF
	echo LCM-DU-Manifest-Version: 1.0 >> META-INF/MANIFEST.MF
	echo LCM-DU-Vendor: technicolor.com >> META-INF/MANIFEST.MF
	echo LCM-DU-Name: $COMPONENT_NAME >> META-INF/MANIFEST.MF
	echo LCM-DU-Version: 0.0.0 >> META-INF/MANIFEST.MF
	if [ -n "$TRUST_LEVEL" ]
	then
		echo LCM-Trust-Level: $TRUST_LEVEL >> META-INF/MANIFEST.MF
	fi
	if [ -n "$RESOURCE_GROUP" ]
	then
		echo LCM-Resource-Group: $RESOURCE_GROUP >> META-INF/MANIFEST.MF
	fi
	
	echo  >> META-INF/MANIFEST.MF
fi

while [ $# -ne 0 ]
do
#   echo $# $1
   run=""
   for elem in `echo $1 | sed "s:/: :g"`
   do

      label=`echo $elem | grep : | sed "s/.*:[0-9]*-*\(.*\)/\1/g"`
      mode=`echo $elem | grep : | sed "s/.*:\([0-9]*\)-*.*/\1/g"`
      file=`echo $elem | sed "s/:.*//g"`

      if [ -n "$run" ]
      then
          run=$run"/"$file
      else
          run=$file
      fi

      if [ -n "$mode" ]
      then 
         chmod $mode $run
      fi

      if [ -n "$MAKE_MANIFEST" ]
      then
	      if [ -n "$label" ]
	      then
		      if [ -d $run ]
		      then
			      echo Name: $run/ >> META-INF/MANIFEST.MF
		      else
			      echo Name: $run >> META-INF/MANIFEST.MF
		      fi
		      echo LCM-Trust-Level: $label >> META-INF/MANIFEST.MF
		      echo >> META-INF/MANIFEST.MF
	      fi
      fi

 done

 shift
done
