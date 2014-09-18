BASE=../../src
CO=${BASE}/co
SRC=${CO}/skiplist.c ${CO}/random.c
# ${CO}/error.c ${CO}/pool.c
INC=-I ${BASE}/include -I ../../api/headers
DEFS= -DUSE_MALLOC -DCDR_ONLY
#-DVERBOSE -DSMALL_SAMPLE 
#-DDDS_DEBUG

all:		skiplist

skiplist:	main.c ${SRC} ${CO}/ ${BASE}/include/skiplist.h
		gcc -O3 -g ${INC} ${DEFS} main.c ${SRC} -o skiplist

clean:
		rm -f skiplist  tags

${BASE}/include/skiplist.h:
