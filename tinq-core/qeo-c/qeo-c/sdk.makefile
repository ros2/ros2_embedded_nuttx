PLATFORM = i686-linux
CC = gcc

TARGET = __TARGET__
LIBDIR = $(QEO_HOME)/c/lib/$(PLATFORM)
LIBS = -lqeo -lqeoutil
CFLAGS = -m32 -g -Wall __CFLAGS__ -I$(QEO_HOME)/c/include

ifeq ($(QEO_HOME),)
$(error QEO_HOME is not defined)
endif

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) -pthread -m32 -L$(LIBDIR) -Wl,-rpath,$(LIBDIR) $(OBJECTS) -Wall $(LIBS) -o $@
	@echo "$(TARGET) created."

clean:
	-rm -f *.o
	-rm -f $(TARGET)
