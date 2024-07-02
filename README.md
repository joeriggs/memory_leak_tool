# memory_leak_tool
Locate memory leaks in a C program.

- Download and test the memory_leak_tool:

  - Clone the repo to your local Linux computer.

  - Build and test:
    - cd memory_leak_tool
    - make
    - ./test

The ./test program runs a few tests, dumping statistics to /tmp/malloc.log.
It occasionally pauses, thus giving you a chance to view the log data.

- Adding the memory_leak_tool to your application:

  - Include the header file "memory_leak_tool.h" in your source code file.

  - Call "memory_leak_tool_inif()" at the beginning of your program.  For
    example:

        if(memory_leak_tool_init()) {
                fprintf(stderr, "Unable to initialize memory_leak_tool.\n");
                return 1;
        }

  - Link the file "libmemory_leak_tool.a" into your application.

  - Start your application.  The memory_leak_tool does NOT do anything when
    you start your application.  memory_leak_tool_init() will initialize the
    tool, but the does doesn't start gathering data until you tell it to start.

- Manually using the memory_leak_tool:

  You can drive the memory_leak_tool from a shell.  There are 4 commands that
  you can run:

  - "touch /tmp/start" will start the memory_leak_tool.  It will start
    tracking memory allocation and free operations, and will store them
    internally.

  - "touch /tmp/stop" will stop the memory_leak_tool.  All data that has been
    accumulated up to that point is still stored.

  - "touch /tmp/print" will dump the data to the file /tmp/malloc.log.  The
    data is still stored internally.

  - "touch /tmp/reset" will purge all of the data that had been previously
    stored.

  You can also drive the memory_leak_tool programatically by adding function
  calls to your program:

  - memory_leak_tool_start() performs the same operation as described above
    for the "touch /tmp/start" command.

  - memory_leak_tool_stop() performs the same operation as described above for
    the "touch /tmp/stop" command.

  - memory_leak_tool_log_data() performs the same operation as described above
    for the "touch /tmp/print" command.

  - memory_leak_tool_reset() performs the same operation as described above for
    the "touch /tmp/reset" command.

  You can see an example of how these functions are used by looking at the
  test.c test program source file.

