
libbreakpoint_handler.a: breakpoint.o breakpoint_return.o
	ar rcs libbreakpoint_handler.a breakpoint.o breakpoint_return.o

breakpoint.o: breakpoint.c
	gcc -c -O0 -g breakpoint.c -o breakpoint.o

breakpoint_return.o: breakpoint_return.s
	gcc -c -O0 -g breakpoint_return.s -o breakpoint_return.o

clean:
	rm -f *.a *.o

