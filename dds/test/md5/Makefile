all:	md5
	./md5 -x > result.txt
	diff result.txt md5-ref.txt

md5:	md5-test.c ../../src/co/md5.c ../../src/include/md5.h
	gcc -O3 -I ../../src/include md5-test.c ../../src/co/md5.c -o md5

clean:
	rm -f md5 result.txt tags

