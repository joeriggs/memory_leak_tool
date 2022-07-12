test : memory_leak_tool.o test.o
	cc -o test test.o memory_leak_tool.c -l pthread

memory_leak_tool.o : memory_leak_tool.c memory_leak_tool.h
	gcc -c memory_leak_tool.c

test.o : test.c memory_leak_tool.h
	gcc -c test.c

clean :
	rm -f test test.o memory_leak_tool.o

