#######################################################
#
# Makefile for DDS test application.
#
#######################################################

EXECUTABLE = dds
LIBDDS = libdds.so

BASE       = ../..

prog_CSRCS = main.c 
prog_COBJS = ${prog_CSRCS:.c=.o}
prog_CHDRS = ${BASE}/src/include/*.h ${BASE}/api/headers/*.h

prog_OBJS  = ${prog_COBJS}

${EXECUTABLE}:	${LIBDDS} ${prog_OBJS}
	gcc -o ${EXECUTABLE} ${prog_OBJS} ${LDFLAGS}

${LIBDDS}:
	cd ${BASE} && \
	clearmake -w -C gnu -f Makefile_component E=HOSTLINUX && \
	cp /vobs/do_vob1/do_store/dds/HOSTLINUX/libdds.so ./test/waitsets; \
	cd ./test/waitsets

#######################################################
# Setup Compile Environment

CC       = gcc

INC_PATH = -I${BASE}/api/headers -I${BASE}/src/include
LIB_PATH = -L.
LIBS     = -ldds -lrt

DEFINES  = -DDDS_DEBUG

OPTTYPE  = -O0

CFLAGS   = -Wall -Wextra -pedantic ${OPTTYPE} -g ${INC_PATH} ${DEFINES}
LDFLAGS  = ${LIB_PATH} ${LIBS} -g -O0

all: ${EXECUTABLE}

#######################################################
# Rules for cleaning

clean:
	\rm -f ${prog_OBJS} ${EXECUTABLE}

reallyclean: clean
	\rm -f .log_file* ${BASE}/*/tags Makefile.bak

########################################################

# Lines of code

wc:
	\wc Makefile ${prog_CSRCS} ../../co/skiplist.c ../../co/llist.c \
	../../co/random.c ${prog_CHDRS} ../../tools/freeshm.c ../rtps/main.c \
	../disc/main.c ../../trans/raw/rtps_raw.c

# Object sizes

size:

	\size ${prog_OBJS} ${EXECUTABLE}

# Additional dependencies.

depend:
	\makedepend ${INC_PATH} ../*/*.[ch]

# DO NOT DELETE
