#######################################################
#
# Makefile for DDS test applications.
#
#######################################################

# Define our sources and objects
EXECS = linuxappl mgmt comp

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
SECURITY   = ${BASE}/security
NSEC	   = ${BASE}/nsec

SECP       = ../../plugins/secplug
NSECP      = ../../plugins/nsecplug

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
disc_CSRCS = ${DISC}/disc_tc.c ${DISC}/disc_sub.c ${DISC}/disc_pub.c \
	     ${DISC}/disc_msg.c ${DISC}/disc_ep.c ${DISC}/disc_match.c \
	     ${DISC}/disc_sedp.c ${DISC}/disc_spdp.c ${DISC}/disc_main.c \
	     ${DISC}/disc_ctt.c ${DISC}/disc_psmp.c \
	     ${DISC}/disc_cdd.c
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
	     ${CO}/thread.c ${CO}/hash.c ${CO}/strseq.c ${CO}/random.c \
	     ${CO}/nmatch.c ${CO}/ipfilter.c ${CO}/libx.c ${CO}/config.c
#sec_CSRCS  = ${SECURITY}/security.c
sec_CSRCS  = ${NSEC}/sec_main.c ${NSEC}/sec_id.c ${NSEC}/sec_perm.c \
	     ${NSEC}/sec_auth.c ${NSEC}/sec_access.c ${NSEC}/sec_crypto.c \
	     ${NSEC}/sec_cdata.c ${NSEC}/sec_logging.c ${NSEC}/sec_util.c \
	     ${NSEC}/sec_compat.c ${NSEC}/sec_a_std.c  ${NSEC}/sec_p_std.c \
	     ${NSEC}/sec_c_std.c ${NSEC}/sec_a_dtls.c ${NSEC}/sec_p_dtls.c \
	     ${NSEC}/sec_a_qeo.c ${NSEC}/sec_p_qeo.c
#splug_CSRCS= ../secplug/msecplug.c ../secplug/xmlparse.c ../security/engine_fs.c
#splug_CSRCS= ../../test/splug/p_main.c ../../test/splug/p_xml.c \
#	     ../security/engine_fs.c
splug_CSRCS= ${NSECP}/sp_auth.c ${NSECP}/sp_crypto.c \
	     ${NSECP}/sp_cert.c ${NSECP}/sp_cred.c \
	     ${NSECP}/sp_sys_cert.c ${NSECP}/sp_sys_crypto.c ${NSECP}/sp_main.c \
	     ${NSECP}/sp_access.c ${NSECP}/sp_xml.c ${NSECP}/sp_access_db.c \
	     ${NSECP}/sp_access_populate.c ${NSECP}/sp_sys_cert_none.c \
	     ${NSECP}/sp_sys.c
sql_CSRCS  = ${SQL}/scan.c ${SQL}/parse.c ${SQL}/bytecode.c
dbg_CSRCS  = ${DBG}/debug.c
dynip_CSRCS= ${DYNIP}/di_main.c ${DYNIP}/di_linux.c

lib_CSRCS  = ${dds_CSRCS} ${type_CSRCS} ${dcps_CSRCS} ${disc_CSRCS} \
	     ${cache_CSRCS} ${rtps_CSRCS} ${trans_CSRCS} ${co_CSRCS} \
             ${sql_CSRCS} ${dbg_CSRCS} ${dynip_CSRCS} ${sec_CSRCS} \
	     ${splug_CSRCS}

GENSRCS = userTypeSupport.c
GENHDRS = $(GENSRCS:.c=.h)
GENOBJS = ${GENSRCS:.c=.o}

linuxappl_CSRCS = linuxappl.c ${lib_CSRCS}
linuxappl_COBJS = ${linuxappl_CSRCS:.c=.o} 
linuxappl_OBJS = ${GENOBJS} ${linuxappl_COBJS}

mgmt_CSRCS = mgmt.c ${lib_CSRCS}
mgmt_COBJS = ${mgmt_CSRCS:.c=.o}
mgmt_OBJS = ${GENOBJS} ${mgmt_COBJS}

comp_CSRCS = comp.c ${lib_CSRCS}
comp_COBJS = $(comp_CSRCS:.c=.o)
comp_OBJS = $(GENOBJS) $(comp_COBJS)

##########################################################
# Setup Compile Environment
# We assume both gcc and coredx_ddl are in your path!

CC         = gcc

INC_PATH   = -I${BASE}/include -I../../api/headers -I/usr/include/libxml2 -I../../plugins/security/ -I${NSECP}/ -I../../../qeo-c-import/openssl/outputNative/openssl/HOSTLINUX/Debug/src/openssl-1.0.1f/include/
LIB_PATH   = -L../../../qeo-c/output/openssl/HOSTLINUX
LIBS       =

DEFINES    = -DFILT_VMWARE -DPTHREADS_USED -DRTPS_USED -DXTYPES_USED \
	     -DDDS_TYPECODE -DDDS_DYN_IP -DBIGDATA -DDDS_SECURITY \
	     -DDDS_NATIVE_SECURITY

# -DFORCE_MALLOC 
# -DXTYPES_USED -DDDS_IPV6 -DCDD_USED -DDDS_TYPECODE
# -DLOG_FILE
# -DVALGRIND_USED #-DDDS_DEBUG -DLOCK_TRACE
# -DDDS_DEBUG -DRTPS_TRACE -DLOG_FILE

OPTTYPE  = -O3
# -pg

CFLAGS     = -Wall -g ${OPTTYPE} ${INC_PATH} ${DEFINES}
LDFLAGS    = ${LIB_PATH} ${LIBS} -lnsl -lrt -lpthread -lssl -lcrypto -lxml2 -g
# -pg

##########################################################
# Rules for compiling

all: ${EXECS} 

linuxappl: $(GENSRCS) ${linuxappl_OBJS}
	$(CC) $(CFLAGS) -o $@ $(linuxappl_OBJS) $(LDFLAGS)

mgmt: $(GENSRCS) ${mgmt_OBJS}
	$(CC) $(CFLAGS) -o $@ $(mgmt_OBJS) $(LDFLAGS)

comp: $(GENSRCS) ${comp_OBJS}
	$(CC) $(CFLAGS) -o $@ $(comp_OBJS) $(LDFLAGS)

##########################################################
# Rules for generating code from DDL


##########################################################
# Rules for cleaning

clean:
	\rm -f ${linuxappl_OBJS} ${mgmt_OBJS} ${comp_OBJS} ${EXECS} gmon_* results/massif.comp* results/massif.linuxappl results/massif.mgmt results/mp.comp* results/mp.linuxappl results/mp.mgmt
