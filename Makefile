test : libmemory_leak_tool.a breakpoint/breakpoint_handler.a test.o
	cc -o test test.o -L. -Lbreakpoint -lmemory_leak_tool -lbreakpoint_handler -ldl -lpthread -lrt

breakpoint/breakpoint_handler.a: 
	make -C breakpoint

libmemory_leak_tool.a : memory_leak_tool.o
	ar rcs libmemory_leak_tool.a memory_leak_tool.o

memory_leak_tool.o : memory_leak_tool.c memory_leak_tool.h
	gcc -O0 -g -c memory_leak_tool.c -Ibreakpoint

test.o : test.c memory_leak_tool.h
	gcc -O0 -g -c test.c

clean :
	make -C breakpoint clean
	rm -f test test.o memory_leak_tool.o libmemory_leak_tool.a

