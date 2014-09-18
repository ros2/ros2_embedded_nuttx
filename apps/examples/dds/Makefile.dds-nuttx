#######################################################
#
# Makefile for DDS chat application.
#
#######################################################

-include $(TOPDIR)/.config
-include $(TOPDIR)/Make.defs
include $(APPDIR)/Make.defs

# A simple DDS chat example

APPNAME         = dds_chat
PRIORITY        = SCHED_PRIORITY_DEFAULT
STACKSIZE       = 2048


ALT_NUM = 0
ALT_STR =
#ALT_NUM = 1
#ALT_STR = -DALT1
#ALT_NUM = 2
#ALT_STR = -DALT2

EXECUTABLE = chat${ALT_NUM}

BASE       = ../../../dds/src

RTPS       = ${BASE}/rtps
TRANS      = ${BASE}/trans
DISC       = ${BASE}/disc
CACHE      = ${BASE}/cache
DCPS       = ${BASE}/dcps
DDS        = ${BASE}/dds
CO         = ${BASE}/co
DBG        = ${BASE}/dbg
DYNIP	   = ${BASE}/dynip
TYPE       = ${BASE}/xtypes
SQL        = ${BASE}/sql
SECURITY   = ${BASE}/security
NSEC	   = ${BASE}/nsec

SECP       = ../../../dds/plugins/secplug
NSECP      = ../../../dds/plugins/nsecplug

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
             ${CO}/thread.c ${CO}/prof.c ${CO}/hash.c ${CO}/strseq.c \
             ${CO}/nmatch.c ${CO}/random.c ${CO}/ipfilter.c ${CO}/libx.c \
	     ${CO}/config.c ${CO}/cmdline.c
#sec_CSRCS  = ${SECURITY}/security.c
sec_CSRCS  = ${NSEC}/sec_main.c ${NSEC}/sec_id.c ${NSEC}/sec_perm.c \
	     ${NSEC}/sec_auth.c ${NSEC}/sec_access.c ${NSEC}/sec_crypto.c \
	     ${NSEC}/sec_cdata.c ${NSEC}/sec_logging.c ${NSEC}/sec_util.c \
	     ${NSEC}/sec_compat.c ${NSEC}/sec_a_std.c  ${NSEC}/sec_p_std.c \
	     ${NSEC}/sec_c_std.c ${NSEC}/sec_a_dtls.c ${NSEC}/sec_p_dtls.c 
#splug_CSRCS= ${SECP}/msecplug.c ${SECP}/xmlparse.c ../security/engine_fs.c
#splug_CSRCS= ../../test/splug/p_main.c ../../test/splug/p_xml.c \
#	     ../security/engine_fs.c
splug_CSRCS= ${NSECP}/sp_auth.c ${NSECP}/sp_crypto.c \
	     ${NSECP}/sp_cert.c ${NSECP}/sp_cred.c \
	     ${NSECP}/sp_sys_cert.c ${NSECP}/sp_sys_crypto.c ${NSECP}/sp_main.c \
	     ${NSECP}/sp_access.c ${NSECP}/sp_xml.c ${NSECP}/sp_access_db.c \
	     ${NSECP}/sp_access_populate.c ${NSECP}/sp_sys_cert_none.c \
	     ${NSECP}/sp_sys.c
sql_CSRCS  = ${SQL}/scan.c ${SQL}/parse.c ${SQL}/bytecode.c
dynip_CSRCS= ${DYNIP}/di_main.c ${DYNIP}/di_linux.c
dbg_CSRCS  = ${DBG}/debug.c

prog_CSRCS = main.c chat_msg.c ${dds_CSRCS} ${type_CSRCS} ${dcps_CSRCS} ${disc_CSRCS} \
             ${cache_CSRCS} ${dbg_CSRCS} ${sql_CSRCS} ${co_CSRCS}	     
#	      ${sec_CSRCS} ${splug_CSRCS} ${dynip_CSRCS}

#comment the following && don't define RTPS_USED to drop RTPS/Discovery
prog_CSRCS += ${rtps_CSRCS} ${trans_CSRCS}

prog_COBJS = ${prog_CSRCS:.c=.o}
prog_CHDRS = ${BASE}/include/*.h ../../../dds/api/headers/dds/*.h chat_msg.h

prog_OBJS  = ${prog_COBJS}

${EXECUTABLE}:	${prog_OBJS}
		${CC} -o ${EXECUTABLE} ${prog_OBJS} ${LDFLAGS}

# Include NuttX apps typical syntaxis

ASRCS		=
CSRCS		= ${prog_CSRCS}

AOBJS		= $(ASRCS:.S=$(OBJEXT))
COBJS		= $(CSRCS:.c=$(OBJEXT))

SRCS		= $(ASRCS) $(CSRCS)
OBJS		= $(AOBJS) $(COBJS)

ifeq ($(CONFIG_WINDOWS_NATIVE),y)
  BIN		= ..\..\libapps$(LIBEXT)
else
ifeq ($(WINTOOL),y)
  BIN		= ..\\..\\libapps$(LIBEXT)
else
  BIN		= ../../libapps$(LIBEXT)
endif
endif


#######################################################
# Setup Compile Environment

#CC       = gcc
CC       = arm-none-eabi-gcc

NUTTX_BASE = ../../../nuttx
NUTTX_HEADERS = ${NUTTX_BASE}/include
INC_PATH = -I${BASE}/include -I../../../dds/api/headers \
			-I../../../dds/plugins/security/ -I${NSECP}/ \
			-I../../../dds/qeo-c-import/openssl/outputNative/openssl/HOSTLINUX/Debug/src/openssl-1.0.1f/include/ \
			-I${NUTTX_HEADERS}
#-I/usr/include/libxml2 

#LIB_PATH =
LIB_PATH = -L../../../dds/qeo-c-import/openssl/output/install/usr/local/lib
LIBS     = 

DEFINES  = -DNUTTX_RTOS -DPTHREADS_USED -DFORCE_MALLOC -DTTY_NORMAL -DRTPS_USED -DCDD_USED -DDDS_DEBUG \
	   -DEXTRA_STATS -DXTYPES_USED  -DDDS_TYPECODE \
	   -DBIGDATA -DDDS_FORWARD -DMSECPLUG_WITH_SECXML -DNOIPC -DNO_SYSLOG

#-DDDS_TCP
#-DDDS_IPV6
#-DDDS_BUILTINS -
#-DDDS_AUTO_LIVELINESS
#-DDDS_SECURITY
#-DDDS_NATIVE_SECURITY
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
#-DDUMP_LOCATORS
#-DLOG_DOMAIN
#-DVALGRIND_USED
#-DDDS_STATUS
#-DFORCE_MALLOC
#-DFILT_VMWARE 

OPTTYPE  = -O0
# -fprofile-arcs -ftest-coverage

CFLAGS   = -Wall -Wextra -pedantic -Wno-long-long ${OPTTYPE} ${INC_PATH} ${DEFINES} ${ALT_STR} -g
#LDFLAGS  = ${LIB_PATH} ${LIBS} -lnsl -lrt -lpthread -lssl -lcrypto -lxml2 -g
LDFLAGS  = -nodefaultlibs ${LIB_PATH} ${LIBS} -g -L../../../nuttx/lib -lsched \
				 -lgcc -larch -lc -lcxx -lmm

#-nodefaultlibs -lgcc -lc
# -lgcov

###########################################################

ROOTDEPPATH	= --dep-path .

# Common build

VPATH		= 

all: .built
.PHONY: clean depend distclean

$(AOBJS): %$(OBJEXT): %.S
	$(call ASSEMBLE, $<, $@)

$(COBJS): %$(OBJEXT): %.c
	$(call COMPILE, $<, $@)
#	${CC} -c $(CFLAGS) -o ${COBJS} ${LDFLAGS}

.built: $(OBJS)
	$(call ARCHIVE, $(BIN), $(OBJS) ${LDFLAGS})
	@touch .built

ifeq ($(CONFIG_NSH_BUILTIN_APPS),y)
$(BUILTIN_REGISTRY)$(DELIM)$(APPNAME)_main.bdat: $(DEPCONFIG) Makefile
	$(call REGISTER,$(APPNAME),$(PRIORITY),$(STACKSIZE),$(APPNAME)_main)

context: $(BUILTIN_REGISTRY)$(DELIM)$(APPNAME)_main.bdat
else
context:
endif

.depend: Makefile $(SRCS)
	@$(MKDEP) $(ROOTDEPPATH) "$(CC)" -- $(CFLAGS) -- $(SRCS) >Make.dep
	@touch $@

depend: .depend

#clean:
#	$(call DELFILE, .built)
#	$(call CLEAN)

clean:
	\rm -f ${prog_OBJS} ${EXECUTABLE} core.*

distclean: clean
	$(call DELFILE, Make.dep)
	$(call DELFILE, .depend)

-include Make.dep
