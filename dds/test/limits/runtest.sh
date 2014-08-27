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


#TOP=/home/users/delbarj/Revolution/ddstestsuite/acceptance_high_end_tch
TOP=.

#. ../dimitri/lib/rbsPerfLib.sh

VALGRIND=
MS_PRINT=

#if [ 1 ]
#then
VALGRIND=valgrind
MS_PRINT=ms_print
#else
#VALGRIND=/home/users/jvoe/bin/valgrind
#MS_PRINT=/home/users/jvoe/bin/ms_print
#fi
RUN="$VALGRIND --tool=massif --time-unit=ms --massif-out-file=results/massif.%q{NAME} "
#RUN="rbsTrackMem --tool statm --time 500 --output tch "
#RUN=


rm /tmp/acceptStageLoad*

# Start the "consumer" component: mgmt
export GMON_OUT_PREFIX=gmon_mgmt_out
export NAME=mgmt
$RUN ${TOP}/mgmt 0 0 0 1 &
#$RUN ${TOP}/mgmt 0 0 0 0 &

# Start all "producer" components: 1 big linuxappl and 48 small components
export GMON_OUT_PREFIX=gmon_linuxappl_out
export NAME=linuxappl
$RUN ${TOP}/linuxappl 0 0 0 1 &
#$RUN ${TOP}/linuxappl 0 0 0 0 &
for i in {1..48}
do
        export NAME=comp$i
	export GMON_OUT_PREFIX=gmon_comp_out
	$RUN ${TOP}/comp $((500+($i-1)*4)) $((100+($i-1))) $((100+($i-1))) 1 &
#	$RUN ${TOP}/comp $((500+($i-1)*4)) $((100+($i-1))) $((100+($i-1))) 0 &
done

# Wait for everything to stabilize
sleep 130
#sleep 600

# Components will have exited by now.
#killall massif-x86-linux
killall linuxappl
killall comp
killall mgmt

# You will find 50 massif output files named based on the application under test.
# Suggestion: invoke this script in an empty subdirectory for each run of the test.

exit 1

cd results
$MS_PRINT massif.mgmt > mp.mgmt
head -n 32 mp.mgmt > results.txt
$MS_PRINT massif.linuxappl > mp.linuxappl
head -n 32 mp.linuxappl >> results.txt
for i in {1..48}
do
	$MS_PRINT massif.comp$i > mp.comp$i
	head -n 32 mp.comp$i >> results.txt
done
cd ..

