#######################################################
#
# Makefile for DDS test application.
#
#######################################################

EXECUTABLE = tsm

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
SQL        = ${BASE}/sql
DYNIP      = ${BASE}/dynip

dds_CSRCS  = ${DDS}/dds.c ${DDS}/domain.c ${DDS}/locator.c ${DDS}/guid.c \
	     ${DDS}/dds_seq.c
type_CSRCS = ${TYPE}/xtypecode.c ${TYPE}/xtypes.c ${TYPE}/xdata.c \
	     ${TYPE}/xcdr.c ${TYPE}/tsm.c ${TYPE}/pl_cdr.c ${TYPE}/pid.c \
	     ${TYPE}/xtopic.c ${TYPE}/vtc.c
dcps_CSRCS = ${DCPS}/dcps_main.c ${DCPS}/uqos.c ${DCPS}/guard.c
cache_CSRCS= ${CACHE}/cache.c
disc_CSRCS = ${DISC}/disc.c
rtps_CSRCS = ${RTPS}/rtps_main.c ${RTPS}/rtps_mux.c
trans_CSRCS= ${TRANS}/ip/rtps_ip.c ${TRANS}/ip/ri_udp.c ${TRANS}/ip/ri_dtls.c
co_CSRCS   = ${CO}/pool.c ${CO}/sys.c ${CO}/error.c ${CO}/ipc.c ${CO}/timer.c \
	     ${CO}/sock.c ${CO}/skiplist.c ${CO}/str.c ${CO}/heap.c ${CO}/md5.c \
	     ${CO}/db.c ${CO}/handle.c ${CO}/tty.c ${CO}/thread.c ${CO}/log.c \
	     ${CO}/hash.c ${CO}/prof.c ${CO}/strseq.c ${CO}/random.c \
	     ${CO}/nmatch.c ${CO}/ipfilter.c ${CO}/libx.c ${CO}/config.c
sql_CSRCS  = ${SQL}/scan.c ${SQL}/parse.c ${SQL}/bytecode.c
dbg_CSRCS  = ${DBG}/debug.c
dynip_CSRCS= ${DYNIP}/di_main.c ${DYNIP}/di_linux.c

prog_CSRCS = main.c qeo_types.c wifidoctor.c ../cdr/crc32.c ${dds_CSRCS} \
	     ${type_CSRCS} ${dcps_CSRCS} ${disc_CSRCS} ${cache_CSRCS} \
	     ${rtps_CSRCS} ${trans_CSRCS} ${dbg_CSRCS} ${sql_CSRCS} \
	     ${dynip_CSRCS} ${co_CSRCS}
prog_COBJS = ${prog_CSRCS:.c=.o}
prog_CHDRS = ${BASE}/include/*.h ../../api/headers/dds/*.h

prog_OBJS  = ${prog_COBJS}
prog_DEPS  = ${prog_COBJS:.o=.d}

${EXECUTABLE}:	${prog_OBJS}
	gcc -o ${EXECUTABLE} ${prog_OBJS} ${LDFLAGS}

#######################################################
# Setup Compile Environment

CC       = gcc

INC_PATH = -I${BASE}/include -I../../api/headers
LIB_PATH = 
LIBS     = 

DEFINES  = -DRTPS_USED -DPTHREADS_USED -DCDD_USED -DXTYPES_USED -DDDS_IPV6 -DDDS_DEBUG -DDDS_TYPECODE -DDDS_DYN_IP
#-DDS_NO_MCAST
#-DFORCE_MALLOC 
#-DTTY_NORMAL
#-DPROFILE
#-DLOCK_TRACE
#-DLOG_FILE 

OPTTYPE  = 
#-Os

CFLAGS   = -Wall -Wextra -Wno-long-long ${OPTTYPE} -g ${INC_PATH} ${DEFINES}
LDFLAGS  = ${LIB_PATH} ${LIBS} -lnsl -lrt -lpthread -g

all: ${EXECUTABLE}

-include ${prog_DEPS}

#######################################################
# Rules for cleaning

clean:
	\rm -f ${prog_OBJS} ${EXECUTABLE}

reallyclean: clean
	\rm -f .log_file* ${BASE}/*/tags Makefile.bak tags ${prog_DEPS}

########################################################

%.o: %.c
	gcc -c $(CFLAGS) $*.c -o $*.o
	@gcc -MM $(CFLAGS) $*.c > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

# Lines of code

wc:
	\wc Makefile ${prog_CSRCS} ../../co/skiplist.c ../../co/llist.c \
	../../co/random.c ${prog_CHDRS} ../../tools/freeshm.c ../rtps/main.c \
	../disc/main.c ../../trans/raw/rtps_raw.c

# Object sizes

size:

	\size ${prog_OBJS} ${EXECUTABLE}

# Lint (if available)

lint:	${prog_CSRCS}
	\flint std.lnt ${prog_CSRCS}

