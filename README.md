# memory_leak_tool
Locate memory leaks in a C program.

This tool will allow you to analyze your program to see if/where there are memory leaks.  The tools tracks all calls to the following function:

- calloc()

- malloc()

- posix_memalign()

- realloc()

- free()

You can download and test the memory_leak_tool by performing the following simple steps:

- Clone the repo to your local Linux computer.

- Build and test:
  - cd memory_leak_tool
  - make
  - ./test

- ./test runs a few simple tests, dumping statistics to /tmp/malloc.log.  It occasionally pauses, thus giving you a chance to view the log data.

You can add the memory_leak_tool to your application by following the following steps:

- Include the header file "memory_leak_tool.h" in your source code file.

- Call "memory_leak_tool_inif()" at the beginning of your program.  For example:

        if(memory_leak_tool_init()) {
                fprintf(stderr, "Unable to initialize memory_leak_tool.\n");
                return 1;
        }

- Link the file "libmemory_leak_tool.a" into your application.

- Start your application.  The memory_leak_tool does NOT do anything when you start your application.  memory_leak_tool_init() will initialize the tool, but the does doesn't start gathering data until you tell it to start.

There are 2 interfaces to the memory_leak_tool:

- Manually using the memory_leak_tool:

  You can drive the memory_leak_tool from a shell.  There are 4 commands that you can run:

  - "touch /tmp/start" will start the memory_leak_tool.  It will start tracking memory allocation and free operations, and will store them internally.

  - "touch /tmp/stop" will stop the memory_leak_tool.  All data that has been accumulated up to that point is still stored.

  - "touch /tmp/print" will dump the data to the file /tmp/malloc.log.  The data is still stored internally.

  - "touch /tmp/reset" will purge all of the data that had been previously stored.

  You can also drive the memory_leak_tool programatically by adding function calls to your program:

  - memory_leak_tool_start() performs the same operation as described above for the "touch /tmp/start" command.

  - memory_leak_tool_stop() performs the same operation as described above for the "touch /tmp/stop" command.

  - memory_leak_tool_log_data() performs the same operation as described above for the "touch /tmp/print" command.

  - memory_leak_tool_reset() performs the same operation as described above for the "touch /tmp/reset" command.

  You can see an example of how these functions are used by looking at the test.c test program source file.

Reading the /tmp/malloc.log file, and interpreting the results:

- The current allocate/free activity that the memory_leak_tool has accumulated will be dumpted to the file /tmp/malloc.log whenever you do one of the following things:

  - "touch /tmp/print".

  - Call memory_leak_tool_log_data() from within your program.

  - Here is an example of the contents of /tmp/malloc.log:

```
      $ cat /tmp/malloc.log 

      alloc_event_adds 10 (11260) : alloc_event_dels 6 (10948) : diff 4 (312).
      =========================================================
      Add ref count    1: Del ref count    0: Size       1: Callers (3) ( 0x4011cb 0x7f6deee04873 0x400eee ).
      Add ref count    1: Del ref count    0: Size      65: Callers (7) ( 0x7f6deee5a6f8 0x7f6deee51cd5 0x7f6deeed72e3 0x401d47 0x401bbb 0x7f6def3b52de 0x7f6deeedd133 ).
      Add ref count    3: Del ref count    1: Size     123: Callers (3) ( 0x401223 0x7f6deee04873 0x400eee ).
      =========================================================
      Ptr 0x13a74930: Size     123: Callers (3) ( 0x401223 0x7f6deee04873 0x400eee ).
      Ptr 0x13a749c0: Size     123: Callers (3) ( 0x401223 0x7f6deee04873 0x400eee ).
      Ptr 0x7f6de8000d70: Size      65: Callers (7) ( 0x7f6deee5a6f8 0x7f6deee51cd5 0x7f6deeed72e3 0x401d47 0x401bbb 0x7f6def3b52de 0x7f6deeedd133 ).
      Ptr 0x13a73d20: Size       1: Callers (3) ( 0x4011cb 0x7f6deee04873 0x400eee ).
      num_entries 4.  total_bytes_allocated 312.
      =========================================================
      mallinfo() comparison:
```

  The first section of the file contains a 1-line summary of activity:

  - "alloc_event_adds 10 (11260)" tells how many total allocate operations occurred, and how many total bytes were allocated.

  - "alloc_event_dels 6 (10940)" tells how many times "free()" was called from your program, and how many total bytes were freed.

  - "diff 4 (312)" lists the difference between total allocations and total frees.

  The second section of the file is the most important part.  It shows all of the outstanding allocation events that have occurred.  In other words, these are the places where one of the allocation functions was called, but free() has not been called yet.  So these are the potential leaks.

  Each line contains the following information:

  - "Add ref count  3" shows the number of times one of the allocation functions was called.

  - "Del ref count  1" shows the number of times free() was called for one of the chunks of memory that was allocated.  In this particular example, malloc() was called 3 times, and free() was called once.  So there are still 2 allocated chunks of memory that are outstanding.

  - "Size   123" shows the size of the allocated blocks.

  - "Callers (2)" shows the stack backtrace for the allocation.  You should be able to identify the locations within your program where one of the allocation functions was called.  This is the location of the potential memory leak.  The entire backtrace is provided, so you can follow the code path that leads down to the allocation function being called.

  The third sections shows all of the currently outstanding memory allocations.

  Each line contains the following information:

  - "Ptr 0x11111111" is the memory address that was returned from the allocation call.

  - "Size   123" is the size that was requested.

  - "Callers" contains the backtrace from where the allocation call was made.

  There are a couple more sections in the file, but their data isn't particularly useful.

