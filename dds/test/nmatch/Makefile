all:	nmatch
	./nmatch-test > result.txt
	/usr/bin/diff result.txt nmatch-ref.txt

nmatch:	main.c ../../src/co/nmatch.c ../../src/include/nmatch.h
	gcc -o nmatch main.c ../../src/co/nmatch.c -I../../src/include

clean:
	rm -f nmatch result.txt tags

