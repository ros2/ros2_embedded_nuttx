EXECUTABLE = cdr

BASE       = ../../src

#TYPE	= ${BASE}/typecode
TYPE	= ${BASE}/xtypes

#type_CSRCS = ${TYPE}/typecode.c ${TYPE}/cdr.c ${TYPE}/pl_cdr.c ${TYPE}/pid.c
type_CSRCS = ${TYPE}/xtypecode.c ${TYPE}/xtypes.c ${TYPE}/xdata.c \
	     ${TYPE}/xcdr.c ${TYPE}/tsm.c ${TYPE}/vtc.c
# ${TYPE}/pl_cdr.c ${TYPE}/pid.c
aux_CSRCS = ${BASE}/co/skiplist.c ${BASE}/co/random.c ${BASE}/dds/dds_seq.c \
	    ${BASE}/co/md5.c ${BASE}/co/str.c ${BASE}/co/error.c \
	    ${BASE}/co/pool.c ${BASE}/co/heap.c ${BASE}/co/hash.c \
	    ${BASE}/co/sys.c ${BASE}/co/db.c ${BASE}/nsec/sec_main.c
#	    ${BASE}/dcps/uqos.c

crc32 = crc32.c

all:	cdr

SRCS =	main.c ${type_CSRCS} ${aux_CSRCS} ${crc32}

TESTS = arrays.c cstrings.c endianness.c sequences.c structs.c \
	unions.c keys.c dstructs.c darrays.c mutable.c map.c holder.c

prog_CSRCS = ${SRCS} ${TESTS}
prog_COBJS = ${prog_CSRCS:.c=.o}
prog_OBJS  = ${prog_COBJS}

CC = gcc

INC_PATH = -I${BASE}/include -I../../api/headers

CFLAGS   = -Wall -g -DCDR_ONLY -DXTYPES_USED -DNO_LOCKS -DDDS_DEBUG -DDDS_TYPECODE -DDDS_NATIVE_SECURITY -DDDS_SECURITY ${INC_PATH}
# -DFORCE_MALLOC

cdr:	${prog_OBJS}
	gcc -O0 -I ${BASE}/include -I ../../api/headers -I crc32.h ${prog_OBJS} -o cdr -lrt

clean:
	rm -f ${prog_OBJS} cdr tags core.*
