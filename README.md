# Shell-Emulator
A C program that runs command line instructions and returns results similarly to other shells.

It does I/O redirection, allows you to run processes in the background with the & symbol, and also
has a command called "status" which shows the exit value of the most recent command and "cd" to change 
directories.

The shell will wait for foreground commands (commands without the &) to complete before it prompts
for a new one. When you begin a background process the terminal will tell you its process id, and when
that process completes, it will tell you that it completed and its exit value. It also tells you if a file
can't be opened for input.

To run the program, in a linux terminal just run

`smallsh`  

and type the command "exit" to exit the program. 


## Example usage: 

% smallsh
: ls > junk.txt
: status
Exit value 0
: wc < badfile
smallsh: cannot open badfile for input
: status
Exit value 1
: test -f badfile
: status
Exit value 1
: sleep 30 &
background pid is 635
: kill -15 635
Child 635 terminated with signal 15.
:exit
%