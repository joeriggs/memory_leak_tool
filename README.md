# memory_leak_tool
Locate memory leaks in a C program.

ToDo List:

- Shell script test tool, that uses the program test to run tests.  And then
  the shell script analyzes the results.

- Instructions in this README file.
  - How to build.
  - How to add it to your program.
  - How to use it:
    - Programatically.
      - Describe the API.
    - Externally.
      - Describe the file names that you can "touch" in order to instruct
        the memory_leak_tool.

- Fix the tool so that it can be activated immediately in a process.  At this
  time it crashes.  Probably related to an allocate operation before we're
  ready.

- What can we do with mmap() and munmap() calls?  Can we track them?

