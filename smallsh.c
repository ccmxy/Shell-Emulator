#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
char *readLine(void);
char **tokenizeLine (char *line);
int ampersand(char **args);
int lsh_launch(char **args);
int executeCommand(char **args, int block,
	       int input, char *inFile,
	       int output, char *outFile);
int redirectInput(char **args, char **inFile, char **outFile);
int redirectOutput(char **args, char **outFile, char **inFile);

int main() {
	char *line, *outFile, *inFile, **args;
	int result, input, output, block, status, exitStatus, termSig; 
	pid_t pid, kidpid;
	//Loop that runs until exit is called:
	while(1) {
		fflush(stdout); 
		//Redirect input to keyboard once the end of a program 
		// file is reached:
		if (feof(stdin)) {
		//dev/tty is the file system object that represents the
		// controlling console. It is specified as the file name 
		// to reopen the terminal as the input stream. 
  			if (!freopen("/dev/tty", "r", stdin)) { 
    				perror("smallsh: dev/tty");
    				exit(1);
  			}
		}
  		printf(": ");
		fflush(stdout);
		//Get line:
		line = readLine(); 
		//Break line into 2D array of words seperated by spaces:
        	args = tokenizeLine(line); 

    		if(args[0] == NULL){ 
     	 		continue; //Line is empty
		}

  		if(strcmp(args[0], "exit") == 0) { 
    			exit(0); //Command is exit
      		}

		if(args[0][0] == '#'){
			continue; //Line is a comment
		}
		
		if(strcmp(args[0], "cd") == 0){ //Command is cd
  			//if no other args, change working directory
  			//to the one specified in home environmnent variable:
	        	if(args[1]==NULL){
                		chdir(getenv("HOME"));
            		}
			//Else, change working directory to the one specified 
			else{
				chdir(args[1]);
			}			
			continue;
		}

		if(strcmp(args[0], "status") == 0){ //Command is status
			if(pid = wait(&status) == 1){
				perror("wait failed");
				exit(1);
			}
			//If child exited normally...
			if(WIFEXITED(status)){
				exitStatus = WEXITSTATUS(status);//get exit status...
				printf("Exit value %d\n", exitStatus);
			}
			continue;
    		} 

		//block is set to false(1) if an ampersand is found, and true(0) if not.
		// It will be passed into executeCommand as the argument that determines wheter
		//to run the command as background:
    		block = (ampersand(args) == 0);

		//input is set to -1 if error, 1 if input file exists but output file does not, 
		// 2 if both input and output file:
    		input = redirectInput(args, &inFile, &outFile);
    		output = redirectOutput(args, &outFile, &inFile);
 		
		//Call execute command 
		status = executeCommand(args, block, 
	       		input, inFile, 
	       		output, outFile);
		
		//Check for terminated background commands before prompting again:		
		pid_t kidpid;
		while ((kidpid = waitpid(-1, &status, WNOHANG)) > 0) {
        		printf("Child %ld ", kidpid);
			if(WIFEXITED(status)){
				//If child process exited normally...
	  			printf("exited with exit value %d.\n", WEXITSTATUS(status));
				//WEXITSTATUS returns the exit status
			}
			if(WIFSIGNALED(status)){
				//If child process terminated by signal...
				printf("terminated with signal %d.\n", WTERMSIG(status));
				//WTERMSIG returns the terminating signal.
			}
		}  
  	}
}

/*
executeCommand
In: 2D char array pointing to command line 
	arguments seperated by spaces, int that contains
	1 if the last arg is NOT & and 0 if it is,
 	ints which indicate whether or not there is input
	or output, and char arrays which contain the names 
	of input or output files to be used.
Out: Command is executed.
*/
int executeCommand(char **args, int block,
	       int input, char *inFile,
	       int output, char *outFile) {

	int i, result, status, nread, nwritten, file_descriptor;
  	char  buffer[2048];
 	pid_t child_id; 
  	child_id = fork();
  	switch(child_id) {
  		case EAGAIN:
    		perror("smallsh: EAGAIN");
    		return -1;
  	case ENOMEM:
    		perror("smallsh: ENOMEM");
    		return -1;

  	case 0: //child process
		//If both input and output files, open and write input to output
		if(input == 2 || output == 2){
			file_descriptor = open(inFile, O_RDWR);
			if(file_descriptor == -1){
				printf("smallsh: cannot open %s for input\n", inFile);
				exit(1);
			}
			nread = read(file_descriptor, buffer, 1024);
			close(file_descriptor);
			file_descriptor = open(outFile, O_RDWR);
			lseek(file_descriptor, 0, SEEK_END);
			nwritten = write(file_descriptor, buffer, strlen(buffer));
			close(file_descriptor);
			memset(buffer,0,strlen(buffer));
			memset(inFile,0,strlen(inFile));
			memset(outFile,0,strlen(outFile));
			input = 0;
			output = 0;
			exit(0);
      		}
		if(output == 1){ //redirect from stdin
			/*if(!block){
				system(">/dev/null 2>&1");
			}*/
			stdout = freopen(outFile, "w+", stdout);
		}
		if(input == 1){ //redirect from stdout
			file_descriptor = open(inFile, O_RDWR);
			if(file_descriptor == -1){
				printf("smallsh: cannot open %s for input\n", inFile);
				exit(1);
			}
			if(!block){
				system(">/dev/null 2>&1");
			}
			close(file_descriptor);
			freopen(inFile, "r", stdin);
		}	
    	result = execvp(args[0], args);
    	if(result == -1){//if error happened in execution 
		perror("smallsh: ERROR");
		exit(1);
    	}	
    	exit(3);//exit normally
    	break;
 	default://parent process 
		//If there is no "&", wait for the child process:
    		if(block) { 
			//wait for child process to change state
    			result = waitpid(child_id, &status, 0);
			//return status to check on signals in the main loop
			return status; 
 		}
		//If last argument is an "&" do not wait for child process,
		// print name of background pid:
    		if(!block){ 
			status = waitpid(child_id, &status, WNOHANG);//use WNOHANG so process will continue
			printf("background pid is %d\n", child_id);
			return status;	
    		}
   	}
}

/*
ampersand
In: 2D array of char strings
Out: 1 if the last string in  the array begins with "&,"
	0 if not. If ampersand is found as last arg, it is removed 
 	from the passed-in array. 
*/
int ampersand(char **args) {
	int i;
	//for loop to get idx of last arg
	for(i = 1; args[i] != NULL; i++) ;
 		if(args[i-1][0] == '&') {//if last argument is &...
    			args[i-1] = NULL; //remove from list
    			return 1;
  		} 		
		else {
    			return 0;
  		}
	return 0;
}

/*
 Check for input redirection
 Returns 0 if < not found,
 1 if < followed by input file is found but no output file,
 and 2 if format is outputFile < inputFile
 modifies outFile and inFile
 */
int redirectInput(char **args, char **inFile, char **outFile) {
int i, j, file_descriptor;
for(i = 0; args[i] != NULL; i++) {
	if(args[i][0] == '<') {
     		if(args[i+1] != NULL) {	
		*inFile = args[i+1];
      	} 
      	if(args[i-1] != NULL){
		file_descriptor = open(args[i-1], O_RDWR);
			if(file_descriptor != -1){  //file exists
			*outFile = args[i-1];
			return 2;//if input and output arg
		}
     	}
	else {
		return -1;
      	}
	//Remove the < from the list of args:
      	for(j = i; args[j-1] != NULL; j++) {
		args[j] = args[j+2];
      	}
      	return 1; //if only input arg
    	}
}
return 0;
}

/*
 Check for output redirection
 Returns 0 if > not found,
 1 if > followed by output file is found but no input file,
 and 2 if format is inputFile > outputFile
 modifies outFile and inFile
*/
int redirectOutput(char **args, char **outFile, char **inFile) {
	int i, j, file_descriptor;
	FILE *fp;

	for(i = 0; args[i] != NULL; i++) {
		if(args[i][0] == '>') {//if > found
      			if(args[i+1] != NULL) { //and there is arg after it
				*outFile = args[i+1];
       				fp = fopen(args[i+1], "w");
				fclose(fp);
      					if(args[i-1] != NULL) {//if arg before it(input)
 						file_descriptor = open(args[i-1], O_RDWR);
						if(file_descriptor != -1){//and is name of existing file
							*inFile = args[i-1];
							close(file_descriptor);
							return 2;//return 2 for 2 files
						}				
      				//Below: Remove the > from list of arguments
				for(j = i; args[j-1] != NULL; j++) {
				args[j] = args[j+2];
     				} 
				return 1;//input is not a file
      				}
			}	
	 		else {
				return -1;//Error: > followed by nothing
      			}	
    		}
  	}
  	return 0;
}

/*
Readline
In: Standard input
Out: An array of characters up to 2048 characters.
 Reads each character 1-by-1 and allocates memory for each 
 as it does.
*/
char* readLine(void)
{
	int bufsize = 64, position = 0, c;
	char* buffer = malloc(sizeof(char) * bufsize);
  	while (1) {
		c = getchar();
    			if (c == EOF || c == '\n') {
    				buffer[position] = '\0';
    				return buffer;
    			} 
    			else {
      				buffer[position] = c;
    			}
    		position++;
		//If position has exceeded 2048 characters...
		if (position > 2048){ 
			printf("\n Sorry, that's too many characters.");
			exit(1);
		}
    		if (position >= bufsize) {
      			bufsize += 64;
      			buffer = realloc(buffer, bufsize);

    		}
  }//End of while loop
}

/*
tokenizeLine
In: A char array with up to 52 words seperated by spaces.
Out: A 2D array which contains an array of pointers
which point to each word in the original char array
there is a space in the sent in char array.
 The maximum number of tokens is 52.
*/
char** tokenizeLine(char* line)
{
	int bufsize = 64, position = 0;
	char **tokens = malloc(bufsize * sizeof(char*));
	char *ptr;
	//Make ptr point to the beginning of the 
	// passed-in string.
 	ptr = strtok(line, " ");
  	while (ptr != NULL) {
		//Tokens array is assigned to point
		// to the beginning of a token. 
    		tokens[position] = ptr;
    		position++; //Position is incremented.
		if (position > 51){//If maximum arguments exceeded
		printf("Sorry, but you've entered too many arguments.\n");
		}
    		if (position >= bufsize) {
      			bufsize += 64;
      			tokens = realloc(tokens, bufsize * sizeof(char*));
    		}
		//When first argument of strtok is 0,
		// the function continues from where it 
		// left in its last invocation:
		ptr = strtok(0, " ");
	}
	tokens[position] = NULL;
  	return tokens;
}





