EXECUTABLE = pid

BASE       = ../../src

RTPS       = ${BASE}/rtps
TRANS      = ${BASE}/trans
DISC       = ${BASE}/disc
CACHE      = ${BASE}/cache
DCPS	   = ${BASE}/dcps
DDS	   = ${BASE}/dds
CO         = ${BASE}/co
DBG        = ${BASE}/dbg
TYPE       = ${BASE}/xtypes
SQL	   = ${BASE}/sql
SECURITY   = ${BASE}/security
DYNIP      = ${BASE}/dynip


dds_CSRCS  = ${DDS}/dds.c ${DDS}/domain.c ${DDS}/locator.c ${DDS}/guid.c \
	     ${DDS}/dds_seq.c
type_CSRCS = ${TYPE}/xtypecode.c ${TYPE}/xtypes.c ${TYPE}/xdata.c \
             ${TYPE}/xcdr.c ${TYPE}/tsm.c ${TYPE}/pl_cdr.c ${TYPE}/pid.c \
             ${TYPE}/xtopic.c ${TYPE}/builtin.c ${TYPE}/vtc.c
dcps_CSRCS = ${DCPS}/dcps_main.c ${DCPS}/uqos.c ${DCPS}/guard.c
cache_CSRCS= ${CACHE}/cache.c
disc_CSRCS = ${DISC}/disc.c
rtps_CSRCS = ${RTPS}/rtps_main.c ${RTPS}/rtps_mux.c
trans_CSRCS= ${TRANS}/ip/rtps_ip.c ${TRANS}/ip/ri_udp.c ${TRANS}/ip/ri_dtls.c
sec_CSRCS  = ${SECURITY}/security.c
co_CSRCS   = ${CO}/pool.c ${CO}/sys.c ${CO}/error.c ${CO}/ipc.c ${CO}/timer.c \
	     ${CO}/sock.c ${CO}/skiplist.c ${CO}/str.c ${CO}/heap.c ${CO}/md5.c\
	     ${CO}/db.c ${CO}/handle.c ${CO}/tty.c ${CO}/log.c ${CO}/ctrace.c \
	     ${CO}/thread.c ${CO}/prof.c ${CO}/hash.c ${CO}/strseq.c \
	     ${CO}/nmatch.c ${CO}/random.c ${CO}/ipfilter.c ${CO}/libx.c \
	     ${CO}/config.c
sql_CSRCS  = ${SQL}/scan.c ${SQL}/parse.c ${SQL}/bytecode.c
dbg_CSRCS  = ${DBG}/debug.c
dynip_CSRCS= ${DYNIP}/di_main.c ${DYNIP}/di_linux.c

tst_CSRCS  = main.c

prog_CSRCS = ${tst_CSRCS} ${dds_CSRCS} ${type_CSRCS} ${dcps_CSRCS} ${disc_CSRCS} \
	     ${cache_CSRCS} ${dbg_CSRCS} ${sql_CSRCS} ${dynip_CSRCS} ${co_CSRCS} \
	     ${sec_CSRCS}

#comment the following + don't define RTPS_USED to drop RTPS/Discovery
prog_CSRCS += ${rtps_CSRCS} ${trans_CSRCS}

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

DEFINES  = -DPTHREADS_USED -DRTPS_USED -DDDS_DEBUG -DLOG_FILE -DDDS_IPV6 -DXTYPES_USED -DDDS_TYPECODE -DDDS_DYN_IP -DFORCE_MALLOC -DDDS_SECURITY
#-DTTY_NORMAL 
#-DCTRACE_USED
#-DLOCK_TRACE
#-DSTATIC_DISCOVERY
#-DCDD_USED
#-DLOG_FILE 
#-DRTPS_USED
#-DRTPS_TRACE 
#-DDDS_DEBUG 
#-DLOCK_TRACE
#-DCTRACE_USED
#-DDUMP_LOCATORS
#-DLOG_DOMAIN
#-DVALGRIND_USED
#-DDDS_STATUS
#-DFORCE_MALLOC
#-DFILT_VMWARE 

OPTTYPE  = -O0
# -fprofile-arcs -ftest-coverage

CFLAGS   = -Wall -Wextra -pedantic -Wno-long-long ${OPTTYPE} -g ${INC_PATH} ${DEFINES}
LDFLAGS  = ${LIB_PATH} ${LIBS} -lnsl -lrt -lpthread -lssl -lcrypto -g
# -lgcov

all: ${EXECUTABLE}

#######################################################
# Rules for cleaning

prog_GCDA = ${prog_CSRCS:.c=.gcda}
prog_GCNO = ${prog_CSRCS:.c=.gcno}

clean:
	\rm -f ${prog_OBJS} ${EXECUTABLE} core.* ${prog_GCDA} ${prog_GCNO} *.png *.css

reallyclean: clean
	\rm -f .log_file* ${BASE}/*/tags Makefile.bak tags
	\rm -Rf coverage

covinit:
	mkdir -p coverage
	cd coverage
	lcov -b .. --directory=../../.. --zerocounters

covadd:
	cd coverage
	lcov -b .. --directory=../../.. --capture --output-file api.info

covresult:
	cd coverage
	genhtml api.info
	firefox index.html

# Additional dependencies.

depend:
	\makedepend ${INC_PATH} ../*/*.[ch]
