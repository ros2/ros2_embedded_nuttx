#######################################################
#
# Makefile for DDS test application.
#
#######################################################

BASE       = ../../src

RTPS       = ${BASE}/rtps
TRANS      = ${BASE}/trans
DISC       = ${BASE}/disc
CACHE      = ${BASE}/cache
DCPS	   = ${BASE}/dcps
DDS	   = ${BASE}/dds
CO         = ${BASE}/co
DBG        = ${BASE}/dbg
#TYPE       = ${BASE}/typecode
TYPE       = ${BASE}/xtypes
SQL	   = ${BASE}/sql

dds_CSRCS  = ${DDS}/dds.c ${DDS}/domain.c ${DDS}/locator.c ${DDS}/guid.c \
	     ${DDS}/dds_seq.c
type_CSRCS = ${TYPE}/xtypecode.c ${TYPE}/xtypes.c ${TYPE}/xdata.c \
	     ${TYPE}/xcdr.c ${TYPE}/tsm.c ${TYPE}/pl_cdr.c ${TYPE}/pid.c \
	     ${TYPE}/xtopic.c ${TYPE}/builtin.c
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
	     ${CO}/nmatch.c ${CO}/random.c ${CO}/ipfilter.c ${CO}/libx.c
sql_CSRCS  = ${SQL}/scan.c ${SQL}/parse.c ${SQL}/bytecode.c
dbg_CSRCS  = ${DBG}/debug.c

lib_CSRCS  = ${dds_CSRCS} ${type_CSRCS} ${dcps_CSRCS} ${disc_CSRCS} \
	     ${cache_CSRCS} ${rtps_CSRCS} ${trans_CSRCS} ${dbg_CSRCS} \
	     ${sql_CSRCS} ${co_CSRCS}
com_CSRCS  = test.c ${lib_CSRCS}
com_COBJS  = ${com_CSRCS:.c=.o}

pub_CSRCS  = test_pub.c ${com_CSRCS}
pub_COBJS  = ${pub_CSRCS:.c=.o}

sub_CSRCS  = test_sub.c ${com_CSRCS}
sub_COBJS  = ${sub_CSRCS:.c=.o}

CHDRS      = ${BASE}/include/*.h ../../api/headers/dds/*.h

prog_OBJS  = ${prog_COBJS}

all: test_pub test_sub

test_pub:    test_pub.o ${com_COBJS}
	gcc -o test_pub test_pub.o ${com_COBJS} ${LDFLAGS}

test_sub:    test_sub.o ${com_COBJS}
	gcc -o test_sub test_sub.o ${com_COBJS} ${LDFLAGS}

#######################################################
# Setup Compile Environment

CC       = gcc

INC_PATH = -I${BASE}/include -I../../api/headers
LIB_PATH = 
LIBS     = 

DEFINES  = -DRTPS_USED -DRTPS_TRACE -DDDS_DEBUG -DLOG_FILE -DPTHREADS_USED -DFORCE_MALLOC -DTTY_NORMAL -DXTYPES_USED 
# -DDDS_IPV6
#-DDUMP_LOCATORS
#-DLOG_DOMAIN
#-DCTRACE_USED -DLOCK_TRACE
#-DVALGRIND_USED
#-DDDS_STATUS
#-DFORCE_MALLOC
#-DFILT_VMWARE 

OPTTYPE  = -O0

CFLAGS   = -Wall -Wextra -pedantic -Wno-long-long ${OPTTYPE} -g ${INC_PATH} ${DEFINES}
LDFLAGS  = ${LIB_PATH} ${LIBS} -lnsl -lrt -g


#######################################################
# Rules for cleaning

clean:
	\rm -f test_pub.o test_sub.o ${com_COBJS} test_pub test_sub core*

reallyclean: clean
	\rm -f .log_file* ${BASE}/*/tags Makefile.bak tags vgcore*

########################################################

# Object sizes

size:

	\size ${EXECUTABLE}

# Lint (if available)

lint:	test_pub.c test_sub.c ${com_CSRCS}
	\flint std.lnt test_pub.c test_sub.c ${com_CSRCS}

# Additional dependencies.

depend:
	\makedepend ${INC_PATH} ../*/*.[ch]
