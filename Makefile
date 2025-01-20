
# When building on FreeBSD, use gmake.

OS := $(shell uname)

ifeq ($(OS),FreeBSD)
TARGETS := libmemory_leak_tool.a test.o
CC := cc
CFLAGS := -D__FREEBSD__
LIBS := -lmemory_leak_tool -ldl -lpthread -lrt -lexecinfo
else
TARGETS := libmemory_leak_tool.a breakpoint/breakpoint_handler.a test.o
CC := gcc
CFLAGS := -D__FREEBSD__
LIBS := -lmemory_leak_tool -ldl -lpthread -lrt
endif

test : $(TARGETS)
	$(CC) -o test test.o -L. $(LIBS)

ifneq ($(OS),FreeBSD)
breakpoint/breakpoint_handler.a: 
	make -C breakpoint
endif

libmemory_leak_tool.a : memory_leak_tool.o
	ar rcs libmemory_leak_tool.a memory_leak_tool.o

memory_leak_tool.o : memory_leak_tool.c memory_leak_tool.h
	$(CC) $(CFLAGS) -O0 -g -c memory_leak_tool.c -Ibreakpoint -fPIC

test.o : test.c memory_leak_tool.h
	$(CC) $(CFLAGS) -O0 -g -c test.c

clean :
ifneq ($(OS),FreeBSD)
	make -C breakpoint clean
endif
	rm -f test test.o memory_leak_tool.o libmemory_leak_tool.a

