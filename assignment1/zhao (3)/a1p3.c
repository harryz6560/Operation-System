#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

int main(int argc, char *argv[]){
	
	fprintf(stdout, "The users choice: %s\n", argv[1]);
	// assumed values
	int MAXLINE = 128; // max charcters in an input line
	int MAX_NTOKEN = 16; // max tokens in any input line
	int MAXWORD = 20; // max characters in any token
	int NPROC = 5; //max commands
	
	char buf[MAXLINE];
	char commands[NPROC][MAXLINE]; // stores all the command strings
	char token_commands[NPROC][MAX_NTOKEN][MAXWORD]; // stores all the token of commands
	int command_counts[NPROC]; // stores the amount of token in each command
	int parent = 0;
	char quit_string[MAXLINE];
	pid_t pids[NPROC];
	int i;
	int count_down = 0;
	int command_index = 0;
	
	// step 1 the start time
	clock_t st_time;
	clock_t en_time;
	struct tms st_cpu;
	struct tms en_cpu;
	
	st_time = times(&st_cpu);
	
	
	// step 2 loop through all the text lines
	while (fgets(buf, MAXLINE, stdin) != NULL) {
		if (buf[0] == '\n'){
			// empty line skipped
		}
		else if (buf[0] == '#'){
			// comment skipped
		}
		else{
			// copy the command
			buf[strlen(buf) - 1] = 0;
			strcpy(commands[command_index], buf);
			
			// token thease commands
			// token variables
			char *token;
			int count = 0;
			char WSPACE[] = "\n \t";
			char test_str[MAXLINE];
			strcpy(test_str, buf);
		
			token = strtok(test_str, WSPACE); // the first token
			while(token != NULL){
				strcpy(token_commands[command_index][count], token);
				count =  count+1;
				token = strtok(NULL, WSPACE);
			}
			command_counts[command_index] = count;
			command_index = command_index + 1;
			 // fprintf(stdout, "length of string: %ld\n", strlen(buf));
			fprintf(stdout, "print_cmd(): [%s]\n", buf);
		}
	}
	
	fprintf(stdout, "\n");
	
	// step 3 execute the commands
	for( i =0; i < command_index; i++){

		// create the childs 
		if ((pids[i] = fork()) < 0) {
			fprintf(stdout, "%s\n", "fork error");
			exit(0);
		} 
		else if (pids[i] == 0) { // child process
			// get the number to tokens in a command
			int count = command_counts[i];
			int error_status = 0;
			// see how many arguments it has
			if(count == 1)
				error_status = -1;
			else if(count == 2)
				error_status = execlp(token_commands[i][0], token_commands[i][0], token_commands[i][1], (char *) NULL);
			else if(count == 3)
				error_status = execlp(token_commands[i][0], token_commands[i][0], token_commands[i][1], 
				token_commands[i][2], (char *) NULL);
			else if(count == 4)
				error_status = execlp(token_commands[i][0], token_commands[i][0], token_commands[i][1], 
				token_commands[i][2], token_commands[i][3], (char *) NULL);
			else {
				error_status = execlp(token_commands[i][0], token_commands[i][0], token_commands[i][1], 
				token_commands[i][2], token_commands[i][3], token_commands[i][4], (char *) NULL);
			}
			
			if (error_status == -1){
				fprintf(stdout, "child (%ld): unable to execute '%s'\n", (long) getpid(), commands[i]);
			}
			
			exit(0);
		}
		else{
			if( *argv[1] == '-'){
				count_down = count_down +1;
			}
		}
	}
	
	// print the process table
	fprintf(stdout, "\nProcess table:\n");
	for(int i = 0; i< command_index; i++){
		fprintf(stdout, "%d [%ld: '%s']\n", i, (long) pids[i], commands[i]);
	}
	fprintf(stdout, "\n");
	
	// the last step for parent
	// step 4
	if (*argv[1] == '0'){
		// do not wait
		sleep(0);
	}
	else if (*argv[1] == '1'){
		// wait for any child terminate
		int status;
		pid_t pid;
		
		while(pid = waitpid(-1, &status, 0) < 0);
		
		// find the index of the command
		int j = 0;
		while (j < NPROC){
			if (pids[j] == pid){
				// we found the index of the pid
				break;
			}
			j = j +1;
		}
		fprintf(stdout, "process (%ld:'%s') exited (status = %d)\n", (long)pid,  commands[j], status);
	}
	else{	// it is -1
		// wait for all child to terminate
		int status;
		pid_t pid;
		
		while (count_down > 0){
			pid = waitpid(-1, &status, 0);
			
			// find the index of the command
			int j = 0;
			while (j < NPROC){
				if (pids[j] == pid){
					// we found the index of the pid
					break;
				}
				j = j +1;
			}
			
			if (status >= 0) 
				fprintf(stdout, "process (%ld:'%s') exited (status = %d)\n", (long)pid, commands[j], status);
			

			count_down = count_down -1;
		} 
	}
	
	static long click = 0;
	click = sysconf(_SC_CLK_TCK);
	
	// step 5
	fprintf(stdout, "\n");
	fprintf(stdout, "Recorded time\n");
	en_time = times(&en_cpu);
    	fprintf(stdout, "Real: %7.2f\nUser: %7.2f\nSys: %7.2f\n",
	(double)(en_time - st_time)/click,
	(double)(en_cpu.tms_utime - st_cpu.tms_utime)/click,
	(double)(en_cpu.tms_stime - st_cpu.tms_stime)/click);	
	
	// print the child process times
	fprintf(stdout, "Child user: %7.2f\nChild sys: %7.2f\n",
	(double)(en_cpu.tms_cutime - st_cpu.tms_cutime)/click,
	(double)(en_cpu.tms_cstime - st_cpu.tms_cstime)/click);
	
	exit(0);
}
