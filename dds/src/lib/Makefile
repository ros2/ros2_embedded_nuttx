#######################################################
#
# Makefile for DDS library.
#
#######################################################

DDS_LIBS = libdds.a libdds.so.1.0.1

all: ${DDS_LIBS}

BASE       = ..

RTPS       = ${BASE}/rtps
TRANS      = ${BASE}/trans
DISC       = ${BASE}/disc
CACHE      = ${BASE}/cache
DCPS	   = ${BASE}/dcps
DDS	   = ${BASE}/dds
CO         = ${BASE}/co
DBG        = ${BASE}/dbg
TYPE       = ${BASE}/xtypes
#TYPE       = ${BASE}/typecode
SQL	   = ${BASE}/sql

dds_CSRCS  = ${DDS}/dds.c ${DDS}/domain.c ${DDS}/locator.c ${DDS}/guid.c \
	     ${DDS}/dds_seq.c
type_CSRCS = ${TYPE}/xtypecode.c ${TYPE}/xtypes.c ${TYPE}/xdata.c \
             ${TYPE}/xcdr.c ${TYPE}/tsm.c ${TYPE}/pl_cdr.c ${TYPE}/pid.c \
             ${TYPE}/xtopic.c ${TYPE}/builtin.c ${TYPE}/vtc.c
#type_CSRCS = ${TYPE}/typecode.c ${TYPE}/cdr.c ${TYPE}/pl_cdr.c ${TYPE}/pid.c
dcps_CSRCS = ${DCPS}/dcps_main.c ${DCPS}/uqos.c ${DCPS}/guard.c
cache_CSRCS= ${CACHE}/cache.c
disc_CSRCS = ${DISC}/disc.c
rtps_CSRCS = ${RTPS}/rtps_main.c ${RTPS}/rtps_mux.c
trans_CSRCS= ${TRANS}/ip/rtps_ip.c ${TRANS}/ip/ri_udp.c ${TRANS}/ip/ri_dtls.c
co_CSRCS   = ${CO}/pool.c ${CO}/sys.c ${CO}/error.c ${CO}/ipc.c ${CO}/timer.c \
	     ${CO}/sock.c ${CO}/skiplist.c ${CO}/str.c ${CO}/heap.c ${CO}/md5.c \
	     ${CO}/db.c ${CO}/handle.c ${CO}/tty.c ${CO}/log.c ${CO}/ctrace.c \
	     ${CO}/thread.c ${CO}/prof.c ${CO}/hash.c ${CO}/strseq.c \
	     ${CO}/nmatch.c ${CO}/random.c ${CO}/libx.c ${CO}/ipfilter.c
sql_CSRCS  = ${SQL}/scan.c ${SQL}/parse.c ${SQL}/bytecode.c
dbg_CSRCS  = ${DBG}/debug.c

lib_CSRCS  = ${dds_CSRCS} ${type_CSRCS} ${dcps_CSRCS} ${disc_CSRCS} \
	     ${cache_CSRCS} ${rtps_CSRCS} ${trans_CSRCS} ${dbg_CSRCS} \
	     ${sql_CSRCS} ${co_CSRCS}
lib_COBJS  = ${lib_CSRCS:.c=.o}
lib_CHDRS  = ${BASE}/include/*.h ../../api/headers/dds/*.h

lib_OBJS   = ${lib_COBJS}

libdds.a: ${lib_OBJS}
	ar rcs  libdds.a ${lib_OBJS}

libdds.so.1.0.1: ${lib_OBJS}
	gcc -shared -Wl,-soname,libdds.so.1 -o libdds.so.1.0.1 ${lib_OBJS}

#######################################################
# Setup Compile Environment

CC       = gcc

INC_PATH = -I${BASE}/include -I../../api/headers
LIB_PATH = 
LIBS     = 

DEFINES  = -DRTPS_TRACE -DDDS_DEBUG -DLOG_FILE -DPTHREADS_USED -DXTYPES_USED
# -DCTRACE_USED
#-DLOCK_TRACE -DPROFILE
#-DVALGRIND_USED
# -DDDS_STATUS
#-DFORCE_MALLOC
#-DFILT_VMWARE 

OPTTYPE  = -O0 -fPIC

CFLAGS   = -Wall -Wextra -Wno-long-long -pedantic ${OPTTYPE} -g ${INC_PATH} ${DEFINES}
LDFLAGS  = ${LIB_PATH} ${LIBS} -lnsl -lrt -g

all: ${EXECUTABLE}

#######################################################
# Rules for cleaning

clean:
	\rm -f ${lib_OBJS} ${DDS_LIBS}

reallyclean: clean
	\rm -f .log_file* ${BASE}/*/tags Makefile.bak

########################################################

# Object sizes

size:

	\size ${lib_OBJS} ${DDS_LIBS}

# Lint (if available)

lint:	${lib_CSRCS}
	\flint std.lnt ${lib_CSRCS}

# Additional dependencies.

depend:
	\makedepend ${INC_PATH} ${lib_CSRCS} ${lib_CHDRS}
