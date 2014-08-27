
all: idlparser

idllexer.c: idllexer.l
	flex -Pidl -o$@ $<

idlparser.c idlparser.h: idlparser.y
	bison -rall -g --name-prefix idl -t -d -o $(basename $(@F)).c $< 

cdump.o: cdump.c cdump.h typeobj.h idlparser_support.h
defsampledump.o: defsampledump.c defsampledump.h typeobj.h idlparser_support.h
idldump.o: idldump.c idldump.h typeobj.h idlparser_support.h
idllexer.o: idllexer.c idlparser.h
idlparser.o: idlparser.c
idlparser_support.o: idlparser_support.c typeobj.h typeobj_manip.h
tsmdump.o: tsmdump.c tsmdump.h typeobj.h 
typeobj_manip.o: typeobj_manip.c typeobj_manip.h typeobj.h
string_manip.o: string_manip.c string_manip.h
main.o: main.c

idlparser: idllexer.o idlparser.o idldump.o typeobj_manip.o tsmdump.o cdump.o idlparser_support.o defsampledump.o string_manip.o main.o
	gcc -g $^ -o $@ `pkg-config --libs glib-2.0`

clean:
	rm -f *.o idlparser.c idllexer.c idlparser.h *~ tags idlparser.dot  idlparser.output  idlparser
	ctags -R

tags: 
	ctags -R

indent:
	indent -linux *.[ch]

%.o: %.c
	gcc -O0 -g -Wall `pkg-config --cflags glib-2.0` -c $< -o $@
