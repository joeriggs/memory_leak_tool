
This program allows you to set a breakpoint in an executing process.  The breakpoint is similar to how GDB can interrupt your program and allow you to take control.

NOTE: This has only been successfully tested on Ubuntu 20.04 x86_64.  Other platforms might be added later.

Here's how it works:

1. Call "breakpoint_handler_init()" to initialize the breakpoint_handler.  The handler registers a SIGTRAP handler (i.e. an "INT 3" handler).

2. Call "breakpint_handler_set()", which inserts a breakpoint (an "INT 3" instruction) into the beginning of the function that we want to intercept and track.  Inserting the "INT 3" allows us to inject our breakpoint_handler() into the program flow each time the tracked function is called, thus allowing us to perform some pre-processing before the function runs.

3. When the tracked function is called, the "INT 3" instruction is executed, and that causes the kernel to pass control to breakpoint_handler().  breakpoint_handler() performs the following steps:

   - Save the return address that the tracked function is supposed to return back to.  The address is saved in "func_retaddr".

   - Replace the return address with the address of breakpoint_return() (which which is located in breakpoint_return.s).  Injecting breakpoint_return() into the execution flow will will allow us to do some post-processing after the tracked function returns, and before control is passed back to the code that originally called the function.

4. breakpoint_handler() returns, and the kernel passes control back to the tracked function.  The function executes and does its thing.

5. When the function completes, it returns back to breakpoint_return() (because we injected breakpoint_return() into the stack back in step #3).

6. breakpoint_return() receives the return value from the tracked function in the %rax register.  This allows us to view the return value before it is passed back to the original caller.

7. breakpoint_return() then returns back to the return address of the function that originally called the tracked function.

At this point we have successfully intercepted the call to the tracked function (thus allowing for pre-processing), and we've successfully intercepted returning from the tracked function (thus allowing for post-processing).

These are the files that comprise this project:

- breakpoint.c

  - This file contains most of the breakpoint-handling code.  It contains the following functions:

    - breakpoint_handler_init()

    - breakpoint_handler_set()

    - breakpoint_handler()

- breakpoint.h

  - This file defines the (very simple) API that the breakpoint_handler provides.

- breakpoint_internal.h

  - This file defines data that is used internally by the breakpoint_handler.  This data is NOT part of the exported API.

- breakpoint_return.c

  - This file contains the following function:

    - breakpoint_return()

  - NOTE: This file is NOT built into the breakpoint library.  Instead, you use it to create the breakpoint_return.s file.  Here's what we do:

    - "gcc -S breakpoint_return.c".  This command compiles breakpoint_return.c and generates an assembly listing file called breakpoint_return.s.

    - Edit breakpoint_return.s.  Locate the "breakpoint_return:" label, and replace the assembly code with code that is similar to the following:

            movq    %fs:mmap_retaddr@tpoff, %rbx
            push    %rbx
            ret

    - Save breakpoint_return.s.  It is now ready to build into the breakpoint_handler library.

- breakpoint_return.s

  - This file contains the code that enables the post-processing operation.  It's written in assembly in order to avoid all of the preamble and postamble (is "postamble" a word?) stuff that is normally placed in a C function.

- Use it in your own program:

To use this breakpoint_handler in your own program, you need to perform the following steps:

1. Build breakpoint_handler.a by running "make".

2. Provide your own pre-processor and post-processor functions.  Both are optional.  You only have to provide the one(s) that you require.

3. From your program, call the following 2 functions:

   A. breakpoint_handler_init() to initialize the breakpoint_handler.

   B. breakpoint_handler_set() to set the breakpoint and register you pre-processor and post-processor functions.

And that's it!  You should be able to inject your pre and post processors into the flow of another function.

