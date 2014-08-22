#set -x
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

#echo "Check $1 $2"
if [ ! -f $2 ] ; then # new Clearcase file
	d=`dirname $2`
	if [ -e $d ] ; then
#		echo -n "create $2 ? [n/y] "
#		read ch
#		if [ $ch = 'y' ] ; then
			echo "cleartool co -unr -nc $d"
			echo "cleartool mkelem $2"
#		fi
	else
		echo "directory $d doesn't exist yet!"
	fi
elif [ ! -f $1 ] ; then # unknown mirror file
	d=`dirname $1`
	if [ ! -d $d ] ; then
		echo "mkdir $d"
	fi
	echo "cp $2 $1"
else			# both files exist -- check if equal!
	`diff $1 $2 > /tmp/dres`
	if [ -s "/tmp/dres" ] ; then
		echo "$1 $2"
	fi
fi
