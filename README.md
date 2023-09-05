# Small-Unix-Shell

A basic command line interface shell written in C.

## Features Implemented
- Interactive prompt input
- Command line input parsing into semantic tokens
- Parameter expansion
    - "~/" is replaced with HOME envrionment variable
    - "$$" is replaced with process ID of shell process
    - "$?" is replaced with exit status of last foreground command
    - "$!" is replaced with process ID of most recent background process
-  Shell built-in commands - exit and cd
-  Non-built-in commands are executed based on PATH environment variable
    - I/O redirection using "<" and ">" operators
    - Background command execution using the "&" operator
-  Custom signal handling
    - SIGSTP is ignored by the shell
    - SIGINT is ignored by the shell at all times except when reading a line of input

 ## How to build and run
 1) Place "smallsh.c" and "makefile" in a directory on your machine.
 2) Run the command "make" inside the directory to generate a executable binary of the shell.
 3) Start up the shell using the command "./smallsh". 
