#######################################################
#
# Makefile for DDS test application.
#
# For DTLS-based security: set sec_CSRCS=SECURITY and splug_CSRCS=../../secplug/*
# For Native DDS security: set sec_CSRCS=NSEC, splug_CSRCS=../../nsecplug and
#                          define DDS_NATIVE_SECURITY
#
#######################################################

EXECUTABLE = dds

BASE       = ../../src

RTPS       = ${BASE}/rtps
TRANS      = ${BASE}/trans
DISC       = ${BASE}/disc
CACHE      = ${BASE}/cache
DCPS	   = ${BASE}/dcps
DDS	   = ${BASE}/dds
CO         = ${BASE}/co
DBG        = ${BASE}/dbg
DYNIP	   = ${BASE}/dynip
TYPE       = ${BASE}/xtypes
SQL	   = ${BASE}/sql
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
	     ${DISC}/disc_cdd.c ${DISC}/disc_policy_updater.c
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
#sec_CSRCS  = ${SECURITY}/security.c
sec_CSRCS  = ${NSEC}/sec_main.c ${NSEC}/sec_id.c ${NSEC}/sec_perm.c \
	     ${NSEC}/sec_auth.c ${NSEC}/sec_access.c ${NSEC}/sec_crypto.c \
	     ${NSEC}/sec_cdata.c ${NSEC}/sec_logging.c ${NSEC}/sec_util.c \
	     ${NSEC}/sec_compat.c ${NSEC}/sec_a_std.c  ${NSEC}/sec_p_std.c \
	     ${NSEC}/sec_c_std.c ${NSEC}/sec_a_dtls.c ${NSEC}/sec_p_dtls.c \
	     ${NSEC}/sec_a_qeo.c ${NSEC}/sec_p_qeo.c ${NSEC}/sec_qeo_policy.c
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

prog_CSRCS = main.c ${dds_CSRCS} ${type_CSRCS} ${dcps_CSRCS} ${disc_CSRCS} \
	     ${cache_CSRCS} ${dbg_CSRCS} ${sql_CSRCS} ${co_CSRCS} \
	     ${dynip_CSRCS} ${sec_CSRCS} ${splug_CSRCS}

#comment the following && don't define RTPS_USED to drop RTPS/Discovery
prog_CSRCS += ${rtps_CSRCS} ${trans_CSRCS}

prog_COBJS = ${prog_CSRCS:.c=.o}
prog_CHDRS = ${BASE}/include/*.h ../../api/headers/dds/*.h

prog_OBJS  = ${prog_COBJS}

${EXECUTABLE}:	${prog_OBJS}
	gcc -o ${EXECUTABLE} ${prog_OBJS} ${LDFLAGS}

#######################################################
# Setup Compile Environment

CC       = gcc

OPENSSL  = ../../../qeo-c-import/openssl/output/openssl/HOSTLINUX/src/openssl-1.0.1g
INC_PATH = -I${BASE}/include -I../../api/headers -I/usr/include/libxml2 -I../../plugins/security/ -I${NSECP}/ -I${OPENSSL}/include/
#LIB_PATH =
#LIB_PATH = -L../../../qeo-c-import/openssl/output/install/usr/local/lib
LIB_PATH = -L../../../qeo-c/output/openssl/HOSTLINUX
LIBS     = 

DEFINES  = -DPTHREADS_USED -DRTPS_USED -DCDD_USED -DDDS_DEBUG -DLOG_FILE \
	   -DXTYPES_USED -DDDS_BUILTINS -DDDS_IPV6 -DDDS_TYPECODE -DDDS_AUTO_LIVELINESS \
	   -DDDS_DYN_IP -DDDS_TCP -DBIGDATA -DRTPS_TRACE -DDDS_TRACE -DDDS_FORWARD \
	   -DEXTRA_STATS -DUSE_BYTESWAP -DDDS_SECURITY -DMSG_TRACE \
	   -DFORWARD_TRC -DFORCE_MALLOC -DUSE_ABORT -DMSECPLUG_WITH_SECXML \
	   -DDDS_NATIVE_SECURITY -DDDS_NO_MCAST -DDTLS_SECURITY \
	   -DUNIX_TERM -DQEO_SECURITY -DDDS_QEO_TYPES
# -DQEO_SECURITY
# -DPROFILE
# -DCTRACE_USED
# -DSEC_TRACE
# -DDDS_NATIVE_SECURITY
# -DTTY_NORMAL
#-DDDS_IP_BCAST
#-DDDS_AUTO_LIVELINESS
#-DFORCE_MALLOC
#-DDDS_IP_BCAST
#-DLOG_FILE -DRTPS_TRACE -DRTPS_SEDP_TRACE
#-DDDS_NO_MCAST
#-DDDS_SERVER
#-DPROFILE 
#-DSTATIC_DISCOVERY
#-DCDD_USED
#-DLOG_FILE 
#-DRTPS_USED
#-DRTPS_TRACE 
#-DDDS_DEBUG 
#-DLOCK_TRACE
#-DCTRACE_USED
#-DCTRACE_START
#-DDUMP_LOCATORS
#-DLOG_DOMAIN
#-DVALGRIND_USED
#-DDDS_STATUS
#-DFORCE_MALLOC
#-DFILT_VMWARE
#-DBIGDATA
#-DMSG_TRACE
#-DDDS_SECURITY
#-DDDS_NATIVE_SECURITY
#-DMSECPLUG_WITH_SECXML 
#-DCOMMON_NAME_ALTERATION
OPTTYPE  = -O0
# -fprofile-arcs -ftest-coverage
PROFILE  =
#-pg
CFLAGS   = -Werror -Wall -Wextra -pedantic -Wno-long-long ${OPTTYPE} ${INC_PATH} ${DEFINES} -g ${PROFILE}
LDFLAGS  = ${LIB_PATH} ${LIBS} -lnsl -lrt -lpthread -lssl -lcrypto -lxml2 -g ${PROFILE}

all: ${EXECUTABLE}

#######################################################
# Rules for cleaning

clean:
	rm -f ${prog_OBJS} ${EXECUTABLE} core.*

reallyclean: clean
	rm -f .tdds_hist .tdds_log_* ${BASE}/*/tags Makefile.bak tags ./.depend cscope.out

########################################################

# Lines of code

wc:
	wc Makefile ${prog_CSRCS} ../../co/llist.c \
	../../co/random.c ${prog_CHDRS} ../../tools/shm/*shm.c ../../tools/ctget.c \
	../bw/main.c ../latency/main.c ../shapes/main.c ../rr/main.c ../cdd/main.c \
	../cdr/*.[ch] ../inst/*.c ../limits/*.[ch] ../md5/*.c ../../trans/raw/rtps_raw.c \
	../api/*.[ch] ../rshapes/main.c

# Object sizes

size:

	size ${prog_OBJS} ${EXECUTABLE}

# coverage

coverage:
	cd ../api/coverage
	lcov -b ../../dcps --directory=../../.. --capture --output-file api.info

# cscope

cscope:
	cscope ${prog_CSRCS}

# Lint (if available)

lint:	${prog_CSRCS}
	${LINT} std.lnt ${prog_CSRCS}

# Additional dependencies.

depend:	.depend
