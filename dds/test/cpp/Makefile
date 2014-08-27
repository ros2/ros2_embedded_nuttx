#######################################################
#
# Makefile for DDS test application.
#
#######################################################

EXECUTABLE = cppdds

BASE       = ../../src

${EXECUTABLE}:	${prog_OBJS}
	g++ -o ${EXECUTABLE} main.cc ${CFLAGS} ${INC_PATH} ${LDFLAGS}

#######################################################
# Setup Compile Environment

CC       = gcc

INC_PATH = -I../../api/headers
LIB_PATH = -L${BASE}/lib
LIBS     = -ldds -lnsl -lrt -g


OPTTYPE  = -O0

CFLAGS   = -Wall -Wextra -pedantic -Wno-long-long ${OPTTYPE} -g ${DEFINES}
LDFLAGS  = ${LIB_PATH} ${LIBS}
all: ${EXECUTABLE}

#######################################################
# Rules for cleaning

clean:
	\rm -f ${prog_OBJS} ${EXECUTABLE} core.*

reallyclean: clean
	\rm -f .log_file* ${BASE}/*/tags Makefile.bak tags

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
