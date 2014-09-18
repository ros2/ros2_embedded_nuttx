#######################################################
#
# Makefile for DDS test applications.
#
#######################################################

# Define our sources and objects


BASE       = ../../src

RTPS       = ${BASE}/rtps
TRANS      = ${BASE}/trans
DISC       = ${BASE}/disc
CACHE      = ${BASE}/cache
DCPS	   = ${BASE}/dcps
DDS	   = ${BASE}/dds
CO         = ${BASE}/co
DBG        = ${BASE}/dbg
DYNIP      = ${BASE}/dynip
TYPE       = ${BASE}/xtypes
SQL        = ${BASE}/sql

dds_CSRCS  = ${DDS}/dds.c ${DDS}/domain.c ${DDS}/locator.c ${DDS}/guid.c \
	     ${DDS}/dds_seq.c ${DDS}/uqos.c ${DDS}/guard.c
type_CSRCS = ${TYPE}/xtypecode.c ${TYPE}/xtypes.c ${TYPE}/xdata.c \
	     ${TYPE}/xcdr.c ${TYPE}/tsm.c ${TYPE}/pl_cdr.c ${TYPE}/pid.c \
	     ${TYPE}/xtopic.c ${TYPE}/builtin.c ${TYPE}/vtc.c
dcps_CSRCS = ${DCPS}/dcps_main.c ${DCPS}/dcps_dpfact.c ${DCPS}/dcps_part.c \
	     ${DCPS}/dcps_pub.c ${DCPS}/dcps_sub.c ${DCPS}/dcps_topic.c \
	     ${DCPS}/dcps_reader.c ${DCPS}/dcps_writer.c ${DCPS}/dcps_event.c \
	     ${DCPS}/dcps_waitset.c ${DCPS}/dcps_builtin.c ${DCPS}/dcps_qos.c \
	     ${DCPS}/dcps_entity.c ${DCPS}/dcps_dbg.c
cache_CSRCS= ${CACHE}/cache.c
disc_CSRCS = ${DISC}/disc.c
rtps_CSRCS = ${RTPS}/rtps_main.c ${RTPS}/rtps_mux.c ${RTPS}/rtps_msg.c \
	     ${RTPS}/rtps_slbw.c ${RTPS}/rtps_slbr.c ${RTPS}/rtps_slrw.c \
	     ${RTPS}/rtps_sfbr.c ${RTPS}/rtps_sfbw.c ${RTPS}/rtps_sfrr.c \
	     ${RTPS}/rtps_sfrw.c ${RTPS}/rtps_clist.c ${RTPS}/rtps_frag.c \
	     ${RTPS}/rtps_dbg.c ${RTPS}/rtps_trace.c ${RTPS}/rtps_ft.c \
	     ${RTPS}/rtps_fwd.c
trans_CSRCS= ${TRANS}/ip/rtps_ip.c ${TRANS}/ip/ri_udp.c ${TRANS}/ip/ri_dtls.c \
	     ${TRANS}/ip/ri_tcp.c ${TRANS}/ip/ri_tcp_sock.c ${TRANS}/ip/ri_tls.c
co_CSRCS   = ${CO}/pool.c ${CO}/sys.c ${CO}/error.c ${CO}/ipc.c ${CO}/timer.c \
	     ${CO}/sock.c ${CO}/skiplist.c ${CO}/str.c ${CO}/heap.c ${CO}/md5.c \
	     ${CO}/db.c ${CO}/handle.c ${CO}/tty.c ${CO}/log.c ${CO}/ctrace.c \
	     ${CO}/thread.c ${CO}/prof.c ${CO}/hash.c ${CO}/strseq.c \
	     ${CO}/nmatch.c ${CO}/random.c ${CO}/ipfilter.c ${CO}/libx.c \
	     ${CO}/config.c ${CO}/cmdline.c
sql_CSRCS  = ${SQL}/scan.c ${SQL}/parse.c ${SQL}/bytecode.c
dbg_CSRCS  = ${DBG}/debug.c
dynip_CSRCS= ${DYNIP}/di_main.c ${DYNIP}/di_linux.c

lib_CSRCS  = ${dds_CSRCS} ${type_CSRCS} ${dcps_CSRCS} ${disc_CSRCS} \
	     ${cache_CSRCS} ${rtps_CSRCS} ${trans_CSRCS} ${co_CSRCS} \
             ${sql_CSRCS} ${dbg_CSRCS} ${dynip_CSRCS}

appl_CSRCS = main.c static.c dynamic.c ${lib_CSRCS}
appl_COBJS = ${appl_CSRCS:.c=.o} 
appl_OBJS =  ${appl_COBJS}

##########################################################
# Setup Compile Environment
# We assume both gcc and coredx_ddl are in your path!

CC         = gcc

INC_PATH   = -I${BASE}/include -I../../api/headers
LIB_PATH   =
LIBS       =

DEFINES    = -DFILT_VMWARE -DPTHREADS_USED -DRTPS_USED -DXTYPES_USED -DDDS_TYPECODE -DDDS_DEBUG -DFORCE_MALLOC
# -DFORCE_MALLOC 
# -DXTYPES_USED -DDDS_IPV6 -DCDD_USED -DDDS_TYPECODE
# -DLOG_FILE
# -DVALGRIND_USED #-DDDS_DEBUG -DLOCK_TRACE
# -DDDS_DEBUG -DRTPS_TRACE -DLOG_FILE

#OPTTYPE  = -O3
# -pg

CFLAGS     = -Wall -g ${OPTTYPE} ${INC_PATH} ${DEFINES}
LDFLAGS    = ${LIB_PATH} ${LIBS} -lnsl -lrt -lpthread -g
# -pg

##########################################################
# Rules for compiling

all: interop 

interop: $(GENSRCS) ${appl_OBJS}
	 $(CC) $(CFLAGS) -o $@ $(appl_OBJS) $(LDFLAGS)

##########################################################
# Rules for generating code from DDL


##########################################################
# Rules for cleaning

clean:
	\rm -f ${appl_OBJS} ${EXECS} gmon_* results/massif.comp* results/massif.linuxappl results/massif.mgmt results/mp.comp* results/mp.linuxappl results/mp.mgmt
