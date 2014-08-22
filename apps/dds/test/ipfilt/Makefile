all:	ipfilt

CO  = ../../src/co
INC = ../../src/include

SRCS = ${CO}/ipfilter.c ${INC}/ipfilter.h main.c

ipfilt:	${SRCS}
	gcc -o ipfilt -g ${SRCS} -I${INC}

clean:
	rm -f ipfilt tags

