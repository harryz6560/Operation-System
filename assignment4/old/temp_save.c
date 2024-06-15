#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <sys/times.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>


// ------------------------------
#define MAXWORD 80
#define MAX_MSG_LEN 132
#define MAXLINE 128
#define MAX_NAME_LEN 32
#define NRES_TYPES 10 
#define NTASKS 25
// ------------------------------

typedef enum {
	WAIT,
	RUN,
	IDLE
} STATUS;

typedef struct {
	char name[MAX_NAME_LEN];
	int units;
} RESOURCE;

typedef struct {
	int index;
	int iter;
} THREAD_ARG;

typedef struct {
	STATUS status;
	char task_name[MAX_NAME_LEN];
	int busy_time;
	int idle_time;
	// name value pair, first name, then value of how many times
	RESOURCE name_value[NRES_TYPES];
	int name_value_count;
} TASK;

// global variable
RESOURCE resources[NRES_TYPES]; // stores all the resources, 0 is name, 1 is value
pthread_mutex_t resource_mutex[NRES_TYPES];
pthread_mutex_t monitor_mutex;
struct TASK tasks[NTASKS]; // stores all of the tasks
pthread_t tids[NTASKS];
int task_count = 0;
int num_recources = 0;
bool printing = false;  // when monitor therad is printing

void* task_thread(void* arg){
	THREAD_ARG* args = (THREAD_ARG*)arg;
	int iter = arg.iter;
	int index = arg.index;
	
	// loop all of the iterations of the tasks
	for(int k=0;k<iter;k++){
		// wait until monitor is not printing
		while(printing == true){
			usleep(1000); // sleep for short druation
		}
		// now go to another iteration
		tasks[index].status = WAIT;
	
		bool continue_loop = true;
		// continue to get the task recources
		while(continue_loop){
			bool all_rescource = false;
			int locked_count = 0;
			// loop through all of the recources the task needs
			for(int j=0;j<tasks[index].name_value_count;j++){
				int aquire_lock = 0;
				int i;
				for(i=0;i<num_recources;i++){
					// check to see if is the same recource and if recource units aaviable is bigger or equal than needed
					if((strcmp(resources[i].name, tasks[index].name_value[j].name) == 0) && resources[i].units >= tasks[index].name_value[j].units){
						aquire_lock = pthread_mutex_trylock(&resource_mutex[i]);
						break;
					}
				}
				
				// if one of them was not aquried
				if(aquire_lock == EBUSY){
					// did not get all of the recources
					all_rescource = false;
					// break the loop no point in coniue to aqurie more recources
					break;
				}
				else if(aquire_lock == 0){
					// got the lock
					locked_count = locked_count +1;
					all_rescource = true;
					
				}
				else{
					printf("Error for getting the lock");
					break;
				}
			}
			
			
			if(all_rescource ==  true){
				for(int j=0;j<tasks[index].name_value_count;j++){
					int i;
					for(i=0;i<num_recources;i++){
						// check to see if is the same recource and if recource units aaviable is bigger or equal than needed
						if((strcmp(resources[i].name, tasks[index].name_value[j].name) == 0) && resources[i].units >= tasks[index].name_value[j].units){
							// get the recource needed
							// we take the recouce we needed
							resources[i].units = resources[i].units - tasks[index].name_value[j].units;
							// release the lock
							pthread_mutex_unlock(&resource_mutex[i]);
						}
					}
				}
				
				// wait until monitor is not printing
				while(printing == true){
					usleep(1000); // sleep for short druation
				}
				
				// change the status to run;
				tasks[index].status = RUN;
				// real time simulating is running
				int busy_time = tasks[index].busy_time*1000;
				usleep(busy_time);
			
				// unlock all of the used recources 
				for(int j=0;j<tasks[index].name_value_count;j++){
					for(int i=0;i<num_recources;i++){
						// check to see if is the same recource
						if(strcmp(resources[i].name, tasks[index].name_value[j].name) == 0){
							// get the mutex lock
							pthread_mutex_lock(&resource_mutex[i]);
							// return all of the used recources
							resources[i].units = resources[i].units + tasks[index].name_value[j].units;
							// unlock the lock
							pthread_mutex_unlock(&resource_mutex[i]);
							break;
						}
					}
				}
				
				// wait until monitor is not printing
				while(printing == true){
					usleep(1000); // sleep for short druation
				}
				
				// wait in real time
				// change status to idel
				tasks[index].status = IDLE;
				int idel_time = tasks[index].idle_time*1000;
				usleep(idel_time);
				
				// break out of the while loop
				continue_loop = false;
				printf("task: %s (tid= %1x, iter= %d, time= %d msec)", tasks[index].task_name, tids[index], k, 0);
			}
			else{	
				// unlock all of the locks taken
				for(int j=0;j<locked_count;j++){
					for(int i=0;i<num_recources;i++){
						// check to see if is the same recource
						if(strcmp(resources[i].name, tasks[index].name_value[j].name) == 0){
							// unlock the lock
							pthread_mutex_unlock(&resource_mutex[i]);
							break;
						}
					}
				}
			
				// wait for this long then try agian
				usleep(10000);
			}
		}
	}
	return NULL;

}

void* monitor_thread(void* arg){
	int monitor_time = (int)arg;
	
	while(1){
		// wait for montor second
		usleep(monitor_time*1000);
		
		printing = true;
		printf(" [WAIT]: ");
		for(int i=0;i<task_count;i++){
			if(tasks[i].STATUS == WAIT){
				printf("%s ", tasks[i].task_name);
			}
		
		}
		printf("\n");
		
		printf(" [RUN]: ");
		for(int i=0;i<task_count;i++){
			if(tasks[i].STATUS == RUN){
				printf("%s ", tasks[i].task_name);
			}
		
		}
		printf("\n");
		
		printf(" [IDLE]: ");
		for(int i=0;i<task_count;i++){
			if(tasks[i].STATUS == IDLE){
				printf("%s ", tasks[i].task_name);
			}
		
		}
		printf("\n");
		printing = false;
	}

	return NULL;
}

int main(int argc, char *argv[]) {
	
	if (argc < 3){ printf("Incorrect Usage!"); exit(0);}
	
	char *input = argv[1];
	int monitorTime = atoi(argv[2]);
	int niter = atoi(argv[3]);
	
	
	FILE *fp;
 	fp = fopen(input, "r");
 	if (fp == NULL){
 		printf("cannot open file!");
 		exit(0);
 	}
	
	char buf[MAXLINE];
	char resources_tokens[NRES_TYPES][MAX_NAME_LEN];
	char task_tokens[NTASKS][5+NRES_TYPES][MAX_NAME_LEN];
	int task_variable_count[NTASKS];
	bool turn_one = true;
	pthread_t monitor_tid;
	
	// loop through all the text lines to get the instructions
	while (fgets(buf, MAXLINE, fp) != NULL) {
		if (buf[0] == '\n'){
			// empty line skipped
		}
		else if (buf[0] == '#'){
			// comment skipped
		}
		else if (buf[0] == '\r'){
			// skip the next time
		}
		else{
			// copy the command
			buf[strlen(buf) - 1] = 0;
			
			// token thease commands
			// token variables
			char *token;
			int count = 0;
			char WSPACE[] = "\n \t";
			char test_str[MAXLINE];
			strcpy(test_str, buf);
			
			if(turn_one == true){
				// turn one is recources so we do not get tasks
				token = strtok(test_str, WSPACE); // the first token
				while(token != NULL){
					strcpy(resources_tokens[count], token);
					count =  count+1;
					token = strtok(NULL, WSPACE);
				}
				num_recources = count;
				// fprintf(stdout, "length of string: %ld\n", strlen(buf));
				//fprintf(stdout, "print_cmd(): [%s]\n", buf);
				turn_one = false;
				
				// set up to get the index of :
				char *e;
				int index;

				// turn all of the recource tokens into resources
				for(int i=0;i<num_recources;i++){
					// get the index of the : slice part
					e = strchr(resources_tokens[i], ':');
					index = (int)(e - resources_tokens[i]);
					
					// get the recource name
					strncpy(resources[i].name, resources_tokens[i], index);
					// get the resource value
					resources[i].units = resources_tokens[i][index+1] - '0';
				}
			}
			else{
				// is all tasks
				token = strtok(test_str, WSPACE); // the first token
				while(token != NULL){
					strcpy(task_tokens[task_count][count], token);
					count =  count+1;
					token = strtok(NULL, WSPACE);
				}
				task_variable_count[task_count] = count;
				// fprintf(stdout, "length of string: %ld\n", strlen(buf));
				//fprintf(stdout, "print_cmd(): [%s]\n", buf);
				
				// trun all of the tokens in to the task type list
				strcpy(tasks[task_count].task_name, task_tokens[task_count][1]);  // the task name
				tasks[task_count].busy_time = task_tokens[task_count][2] - '0';
				tasks[task_count].idle_time = task_tokens[task_count][3] - '0';
				tasks[task_count].name_value_count = count-4;  // number of recources needed
				tasks[task_count].status = WAIT;
				
				// set up to get the index of :
				char *e;
				int index;

				// turn all of the recource tokens into resources
				for(int i=0;i<num_recources;i++){
					// get the index of the : slice part
					e = strchr(task_tokens[task_count][i], ':');
					index = (int)(e - task_tokens[task_count][i]);
					
					// get the recource name
					strncpy(tasks[task_count].name_value[i].name, task_tokens[task_count][i+4], index);
					// get the resource value
					tasks[task_count].name_value[i].units = task_tokens[task_count][i+4][index+1] - '0';
				}

				// increase the number of task added
				task_count = task_count + 1;
			}
		}
	}
	
	// initalize of the mutexes
	for(int i=0;i<NRES_TYPES;i++){
		pthread_mutex_init(&resource_mutex[i], NULL);
	}
	int rval;
	
	// start the monitor thread
	rval = pthread_create(&monitor_tid, NULL, monitor_thread, (void*)(monitorTime));
	
	// start all of the tasks
	THREAD_ARG thread_args[task_count+1];
	for (int i=0;i<task_count;i++){
		thread_args[i].index = i;
		thread_args[i].iter = i;
		rval = pthread_create(&(tids[i]), NULL, task_thread, (void*)(thread_args[i]));
	}
	
	
	//wait for all therad to join back
	for (int i=0;i<task_count;i++){
		pthread_join(tids[i], NULL);
	}
	
	// terminate the monitor thread
	pthread_cancel(monitor_tid);
	pthread_join(monitor_tid, NULL);
	
	// print the end result
	printf("System Resources: \n");
	for(int i= 0;i<num_recources;i++){
		printf("	%s: (maxAvail= %d, held= 0)\n", resources[i].name, resources.units);
	
	}
	
	// print the end result
	printf("System Tasks: \n");
	for (int i=0;i<task_count;i++){
		printf("[%d] %s (%s, runTime= %d msec, idleTime= %d msec): \n", i, tasks[i].task_name, tasks[i].busy_time, tasks[i].idle_time);
		printf("	(tid= %1lx)\n", tids[i]);
		for(int j= 0;j<tasks[i].name_value_count;j++){
			printf("	%s: (maxAvail= %d, held= 0)\n", tasks[i].name_value[j].name, tasks[i].name_value[j].units);
	
		}
		printf("\n");
	}
	
	printf("Running time= %d msec", 0);
	
	return 0;
}
