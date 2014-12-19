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

#
# Small shell script that fakes a compiler, and actually outputs a klocwork
# trace file

OUT=`for i in  "$@" ; do echo -n ";"$i; done`

MD=0
OFILE=a.out

EMULATE_GCC=0
EMULATE_AR=0

if [ "$CONFIGURE_DIR" ]
then
	eval $*
else

	case "$1" in 
		gcc) EMULATE_GCC=1;;
		g++) EMULATE_GCC=1;;
		ar) EMULATE_AR=1
	esac	

	##< Handle GCC commands
	if [ $EMULATE_GCC -eq 1 ]
	then
		COMPILE=0
		INITIAL=1
		VERBOSE=0
		VERSION=0
		while [ $# -gt 0 ]
		do
			case "$1" in
				-c) COMPILE=1; shift;;
				-o) shift; OFILE=$1; shift;;
				-MD) shift; MD=1 ;; 
				-MF) shift; DOFILE=$1; shift ;; 
				--version) shift; VERSION=1 ;; 
				-v) shift; VERBOSE=1 ;; 
				-L) shift; LIBPATHS="$LIBPATHS $1"; shift ;; 
				-l) shift; LIBS="$LIBS $1"; shift ;; 
				-*) shift ;;
				*) if [ $INITIAL -ne 1 ]; then ARGS="$ARGS $1"; fi ; shift ;;
			esac
			INITIAL=0
		done

		if [ "$OFILE" = "a.out" ]
		then
			if [ $COMPILE -eq 1 ]
			then
				OFILE=`echo $ARGS | sed -e "s/\.c$/.o/g" -e "s/\.cc$/.o/g" -e "s/\.cpp$/.o/g" -e "s/\.C$/.o/g"`
			fi
		fi

		if [ $MD -ne 0 ] 
		then 
			if [ "$DOFILE" ]
			then
				touch $DOFILE
			else
				touch `echo $OFILE | sed "s/\.o$/\.d/g"` 
			fi
		fi

		if [ $COMPILE -eq 1 ]
		then 
			echo "exec;;`pwd`;"$OUT > $OFILE
		else
			if [ $VERSION -eq 1 ]
			then
				gcc --version
			else
				if [ $VERBOSE -eq 1 ]
				then
					gcc -v
				else
					cat $ARGS > $OFILE
					echo "exec;;`pwd`;"$OUT >> $OFILE
				fi
			fi
		fi
	fi
	##> Handle GCC commands

	##< Handle AR commands
	if [ $EMULATE_AR -eq 1 ]
	then
		shift # Remove AR from commandline
		shift # Remove operation and modifiers
		OFILE=$1
		shift # Remove archive

		if [ -n "$*" ]
		then
			cat $* > $OFILE
		fi
		echo "exec;;`pwd`;"$OUT >> $OFILE
	fi
	##>

fi
# vim: foldmethod=marker foldmarker=##<,##> :
