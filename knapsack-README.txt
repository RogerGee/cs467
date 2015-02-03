knapsack project by Roger Gee
----------------------------------------------------------------------------------
Building the program:

    [source files]
    knapsack2.c

    The program was written in C. It uses standard C libraries with some
optional libraries for terminal capabilities. The optional libraries are disabled
by default. The C stdio library must support POSIX (ANSI/C99) format strings. This
can be a problem if using a Microsoft implementation of the library.

    If you are building on MS Windows:
       - use MinGW or other GNU compiler variant
       - build without the MS implementation of the C stdio library; this can be
accomplished in several ways:
           $ gcc -std=c99 knapsack2.c
           $ gcc -D__USE_MINGW_ANSI_STDIO knapsack2.c

    If you are building on a POSIX-complient system:
       - build like so
         $ gcc knapsack2.c
       - you can optionally build in the color formatting for the terminal if you
       have 'libtinfo' installed (comes with ncurses-dev)
         $ gcc -DFEAT_LINUX_TINFO -ltinfo knapsack2.c
----------------------------------------------------------------------------------
Running the program:

    The program can accept input in two ways: from stdin or from files specified
on its command-line. The input format for each file is the same. 'stdin' may only
accept a single problem instance.

       Examples:
        $ ./a.out k10.csv k20.csv                           #run k10.csv, then k20.csv
        $ python random-knapsack-instance.py 20 | ./a.out   # accept problem on stdin
--------------------------------------------------------------------------------
