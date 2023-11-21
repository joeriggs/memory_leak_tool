test : libmemory_leak_tool.a test.o
	cc -o test test.o -L. -lmemory_leak_tool -ldl -lpthread

libmemory_leak_tool.a : memory_leak_tool.o
	ar rcs libmemory_leak_tool.a memory_leak_tool.o

memory_leak_tool.o : memory_leak_tool.c memory_leak_tool.h
	gcc -O0 -g -c memory_leak_tool.c

test.o : test.c memory_leak_tool.h
	gcc -O0 -g -c test.c

clean :
	rm -f test test.o memory_leak_tool.o libmemory_leak_tool.a

