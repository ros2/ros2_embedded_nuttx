#######################################################
#
# Makefile for DDS test application.
#
#######################################################

EXECUTABLE = sim

BASE       = ../../src

RTPS       = ${BASE}/rtps
TRANS      = ${BASE}/trans
DISC       = ${BASE}/disc
CACHE      = ${BASE}/cache
DCPS	   = ${BASE}/dcps
DDS	   = ${BASE}/dds
CO         = ${BASE}/co
DBG        = ${BASE}/dbg
TYPE       = ${BASE}/typecode
SIM        = ${BASE}/sim

dds_CSRCS  = ${DDS}/dds.c ${DDS}/domain.c ${DDS}/locator.c ${DDS}/guid.c
type_CSRCS = ${TYPE}/typecode.c ${TYPE}/cdr.c ${TYPE}/pl_cdr.c ${TYPE}/pid.c
dcps_CSRCS = ${DCPS}/dcps_main.c ${DCPS}/qos.c
cache_CSRCS= ${CACHE}/cache.c
disc_CSRCS = ${DISC}/disc.c
rtps_CSRCS = ${RTPS}/rtps_main.c ${RTPS}/rtps_mux.c
trans_CSRCS= ${TRANS}/udpv4/rtps_udpv4.c
co_CSRCS   = ${CO}/pool.c ${CO}/sys.c ${CO}/error.c ${CO}/ipc.c ${CO}/timer.c \
	     ${CO}/sock.c ${CO}/skiplist.c ${CO}/str.c ${CO}/heap.c ${CO}/md5.c \
	     ${CO}/db.c ${CO}/handle.c ${CO}/tty.c
dbg_CSRCS  = ${DBG}/debug.c
sim_CSRCS  = ${SIM}/sim.c

prog_CSRCS = main.c test.c ${dds_CSRCS} ${type_CSRCS} ${dcps_CSRCS} ${disc_CSRCS} \
	     ${cache_CSRCS} ${rtps_CSRCS} ${trans_CSRCS} ${dbg_CSRCS} \
	     ${co_CSRCS} ${sim_CSRCS}
prog_COBJS = ${prog_CSRCS:.c=.o}
prog_CHDRS = ${BASE}/include/*.h ../../api/headers/dds/*.h

prog_OBJS  = ${prog_COBJS}

${EXECUTABLE}:	${prog_OBJS}
	gcc -o ${EXECUTABLE} ${prog_OBJS} ${LDFLAGS}

#######################################################
# Setup Compile Environment

CC       = gcc

INC_PATH = -I${BASE}/include -I../../api/headers
LIB_PATH = 
LIBS     = 

DEFINES  = -DRTPS_TRACE -DDDS_DEBUG -DLOG_FILE -DFILT_VMWARE -DSIMULATION
# -DDDS_STATUS
#-DFORCE_MALLOC

OPTTYPE  = -O0

CFLAGS   = -Wall -Wextra -pedantic ${OPTTYPE} -g ${INC_PATH} ${DEFINES}
LDFLAGS  = ${LIB_PATH} ${LIBS} -lnsl -lrt -g

all: ${EXECUTABLE}

#######################################################
# Rules for cleaning

clean:
	\rm -f ${prog_OBJS} ${EXECUTABLE}

reallyclean: clean
	\rm -f .log_file* ${BASE}/*/tags Makefile.bak

########################################################

# Object sizes

size:

	\size ${prog_OBJS} ${EXECUTABLE}

# Lint (if available)

lint:	${prog_CSRCS}
	\flint std.lnt ${prog_CSRCS}

# Additional dependencies.

depend:
	\makedepend ${INC_PATH} ../*/*.[ch]
