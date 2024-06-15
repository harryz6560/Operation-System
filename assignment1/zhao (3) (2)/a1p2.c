#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>


int main(int argc, char *argv[]){
	int MAXLINE = 128;
	char buf[MAXLINE]; 
	pid_t pid, wpid;
	int status;
	int condition = 1;
	
	pid = fork();
	if (pid < 0) {
		fprintf(stdout, "%s\n", "fork error");
	} else if (pid == 0) { /* child */
		execlp("./myclock", "myclock", "out1", (char *) NULL);
	}
	else{	/* this is the parent process
		the pid returns twice so the parent will get the pid of the child
		and therefore will not go through the child process and the child 
		will not go through the parent process
		*/
		if (*argv[1] == 'w'){
		// wait for the child to end
			while(wpid = waitpid(pid, &status, 0) >0);
			condition = 0;
			if (status == 0)
				printf("the child terminated");
		}
		else if (*argv[1] == 's'){
			// sleep for 2 min an exit
			sleep(120);
			exit(0);
		}
	}

	exit(0);
}
