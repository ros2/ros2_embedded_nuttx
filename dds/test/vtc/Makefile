all:	vtc

BASE = ../../src

TYPE = ${BASE}/xtypes
MISC = ${BASE}/co
DDS  = ${BASE}/dds

type_CSRCS = ${TYPE}/vtc.c ${TYPE}/xtypes.c ${TYPE}/xtypecode.c \
	     ${TYPE}/xdata.c ${TYPE}/xcdr.c ${TYPE}/tsm.c
misc_CSRCS = ${MISC}/skiplist.c ${MISC}/random.c \
	     ${MISC}/md5.c ${MISC}/error.c ${MISC}/pool.c ${MISC}/str.c \
	     ${MISC}/heap.c ${MISC}/hash.c ${MISC}/sys.c ${MISC}/db.c
dds_CSRCS  = ${DDS}/dds_seq.c

SRCS =  vtcdump.c ${type_CSRCS} ${misc_CSRCS} ${dds_CSRCS}

prog_CSRCS = ${SRCS}
prog_COBJS = ${prog_CSRCS:.c=.o}
prog_OBJS  = ${prog_COBJS}

INC_PATH = -I${BASE}/include -I../../api/headers

CFLAGS = -Wall -g -DCDR_ONLY -DXTYPES_USED -DNO_LOCKS -DDDS_DEBUG -DDDS_TYPECODE ${INC_PATH}

vtc:	${prog_OBJS}
	gcc -O0 ${INC_PATH} ${prog_OBJS} -o vtc -lrt

clean:
	rm -f ${prog_OBJS} vtc tags core.*

