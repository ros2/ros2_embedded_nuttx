all:		test_scan test_parse

test_scan:	scan.h scan.c
		gcc scan.c ../co/hash.c -O0 -g -D SCAN_DEBUG -I ../include -o test_scan

CO = ../co
TC = ../typecode

SRCS = ${CO}/hash.c ${CO}/pool.c ${CO}/db.c ${CO}/error.c ${CO}/skiplist.c \
       ${CO}/log.c ${CO}/sys.c ${TC}/cdr.c ${TC}/typecode.c pl_dummy.c

test_parse:	scan.h scan.c parse.h parse.c bytecode.c ${SRCS}
		gcc parse.c scan.c bytecode.c ${SRCS} -O0 -g \
-D PARSE_DEBUG -D NO_LOCKS -D DDS_DEBUG \
-I ../include -I ../export -o test_parse -lrt

clean:
		rm test_parse test_scan

