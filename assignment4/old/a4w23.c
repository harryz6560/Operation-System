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
	RESOURCE units_held[NRES_TYPES];
	double wait_time;
} TASK;

// global variable
RESOURCE resources[NRES_TYPES]; // stores all the resources, 0 is name, 1 is value
pthread_mutex_t resource_mutex[NRES_TYPES];
pthread_mutex_t monitor_mutex;
TASK tasks[NTASKS]; // stores all of the tasks
pthread_t tids[NTASKS];
int task_count = 0;
int num_recources = 0;
bool printing = false;  // when monitor therad is printing
// calculate time
clock_t st_time;
clock_t en_time;
struct tms st_cpu;
struct tms en_cpu;

const char* getStatus(STATUS stat){

	switch(stat){
		case WAIT: return "WAIT";
		case RUN: return "RUN";
		case IDLE: return "IDLE";

	}
}


void* task_thread(void* arg){
	THREAD_ARG* args = (THREAD_ARG*)arg;
	int iter = (*args).iter;
	int index = (*args).index;
	clock_t wait_st_time;
	clock_t wait_en_time;
	struct tms wait_st_cpu;
	struct tms wait_en_cpu;
	static long click = 0;
	click = sysconf(_SC_CLK_TCK);
	
	// loop all of the iterations of the tasks
	for(int k=1;k<iter+1;k++){
		// wait until monitor is printing
		while(printing == true){

		}
		// now go to another iteration
		tasks[index].status = WAIT;
		wait_st_time = times(&wait_st_cpu);
		bool continue_loop = true;
		// continue to get the task recources
		while(continue_loop){
			bool all_rescource = false;
			int locked_count = 0;
			// loop through all of the recources the task needs
			for(int j=0;j<tasks[index].name_value_count;j++){
				int aquire_lock = 1;
				for(int i=0;i<num_recources;i++){
					// check to see if is the same recource and if recource units aaviable is bigger or equal than needed
					if((strcmp(resources[i].name, tasks[index].name_value[j].name) == 0) && resources[i].units >= tasks[index].name_value[j].units){
						aquire_lock = pthread_mutex_trylock(&resource_mutex[i]);
						break;
					}
				}
				
				if(aquire_lock == 0){
					// got the lock
					locked_count = locked_count +1;
					all_rescource = true;
					
				}
				else{
					// did not get all of the recources
					all_rescource = false;
					break;
				}
			}
			
			// printf("%s; %d \n", tasks[index].task_name, all_rescource);
			if(all_rescource ==  true){
				
				bool enough_rescource = true;
				int j;
				for(j=0;j<tasks[index].name_value_count;j++){
					for(int i=0;i<num_recources;i++){
						// check to see if is the same recource and if recource units aaviable is bigger or equal than needed
						if((strcmp(resources[i].name, tasks[index].name_value[j].name) == 0)){
							// get the recource needed
							if(resources[i].units >= tasks[index].name_value[j].units){
								// we take the recouce we needed
								//printf("taking %s:    %s:%d  ", tasks[index].task_name, resources[i].name, resources[i].units);
								resources[i].units = resources[i].units - tasks[index].name_value[j].units;
								tasks[index].units_held[j].units = tasks[index].units_held[j].units + tasks[index].name_value[j].units;
								//printf("taking %s:    %s:%d  \n", tasks[index].task_name, resources[i].name, resources[i].units);
								pthread_mutex_unlock(&resource_mutex[i]);
							}
							else{
								enough_rescource = false;
								pthread_mutex_unlock(&resource_mutex[i]);
								break;
							}
						}
					}
				}
				
				if(enough_rescource){
					wait_en_time = times(&wait_en_cpu);
					double wait_time_temp = (double)(wait_en_time - wait_st_time)/click *1000;
					tasks[index].wait_time = tasks[index].wait_time + wait_time_temp;
					
					//printf("\n");
					
					// wait until monitor is printing
					while(printing == true){
						
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
								//printf("returning %s:    %s:%d  ", tasks[index].task_name, resources[i].name, resources[i].units);
								resources[i].units = resources[i].units + tasks[index].name_value[j].units;
								tasks[index].units_held[j].units = tasks[index].units_held[j].units - tasks[index].name_value[j].units;
								//printf("returning %s:    %s:%d  \n", tasks[index].task_name, resources[i].name, resources[i].units);
								// unlock the lock
								pthread_mutex_unlock(&resource_mutex[i]);
								break;
							}
						}
					}
					// printf("\n");
					
					// wait until monitor is printing
					while(printing == true){
						
					}
					
					// wait in real time
					// change status to idel
					tasks[index].status = IDLE;
					int idel_time = tasks[index].idle_time*1000;
					usleep(idel_time);
					
					// break out of the while loop
					continue_loop = false;
					static long click = 0;
					click = sysconf(_SC_CLK_TCK);
					struct tms en_cpu_temp;
					clock_t end_time = times(&en_cpu_temp);
					double cpu_time_used = (double)(end_time - st_time)/click *1000;
					printf("task: %s (tid= %1lx, iter= %d, time= %.2f msec)\n", tasks[index].task_name, tids[index], k, cpu_time_used);
				}
				else{
					// unlock all of the locks taken
					for(int m=0;m<j;m++){
						for(int i=0;i<num_recources;i++){
							// check to see if is the same recource
							if(strcmp(resources[i].name, tasks[index].name_value[m].name) == 0){
								// return all of the values
								resources[i].units = resources[i].units + tasks[index].name_value[j].units;
								tasks[index].units_held[j].units = tasks[index].units_held[j].units - tasks[index].name_value[j].units;
								// unlock the lock
								pthread_mutex_unlock(&resource_mutex[i]);
								break;
							}
						}
					}
				
				}
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
	int monitor_time = *((int *)arg);
	
	while(1){
		// wait for montor second
		usleep(monitor_time*1000);
		
		printing = true;
		printf("\nmonitor:");
		printf("[WAIT]: ");
		for(int i=0;i<task_count;i++){
			if(tasks[i].status == WAIT){
				printf("%s ", tasks[i].task_name);
			}
		
		}
		printf("\n");
		
		printf("	[RUN]: ");
		for(int i=0;i<task_count;i++){
			if(tasks[i].status == RUN){
				printf("%s ", tasks[i].task_name);
			}
		
		}
		printf("\n");
		
		printf("	[IDLE]: ");
		for(int i=0;i<task_count;i++){
			if(tasks[i].status == IDLE){
				printf("%s ", tasks[i].task_name);
			}
		
		}
		printf("\n");
		printing = false;
		printf("\n");
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
	
	st_time = times(&st_cpu);
	char buf[MAXLINE];
	char resources_tokens[NRES_TYPES][MAX_NAME_LEN];
	char task_tokens[NTASKS][5+NRES_TYPES][MAX_NAME_LEN];
	bool turn_one = true;
	pthread_t monitor_tid;
	
	
	// printf("it worked here!\n");
	// loop through all the text lines to get the instructions
	while (fgets(buf, MAXLINE, fp) != NULL) {
		// printf("%s", buf);
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
				num_recources = count-1;
				// fprintf(stdout, "length of string: %ld\n", strlen(buf));
				//fprintf(stdout, "print_cmd(): [%s]\n", buf);
				turn_one = false;
				
				// set up to get the index of :
				char *e;
				int index;
				
				/*
				for(int i=0;i<num_recources;i++){
					printf("%s\n", resources_tokens[i]);
				}
				*/
				
				// turn all of the recource tokens into resources
				for(int i=0;i<num_recources;i++){
					// get the index of the : slice part
					e = strchr(resources_tokens[i+1], ':');
					index = (int)(e - resources_tokens[i+1]);
					
					// get the recource name
					strncpy(resources[i].name, resources_tokens[i+1], index);
					// get the resource value
					resources[i].units = resources_tokens[i+1][index+1] - '0';
					//printf("%s, %d \n", resources[i].name, resources[i].units);
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
				
				// fprintf(stdout, "length of string: %ld\n", strlen(buf));
				//fprintf(stdout, "print_cmd(): [%s]\n", buf);
				
				// trun all of the tokens in to the task type list
				strcpy(tasks[task_count].task_name, task_tokens[task_count][1]);  // the task name
				tasks[task_count].busy_time =  atoi(task_tokens[task_count][2]);
				tasks[task_count].idle_time =  atoi(task_tokens[task_count][3]);
				tasks[task_count].name_value_count = count-4;  // number of recources needed
				tasks[task_count].status = WAIT;
				
				// set up to get the index of :
				char *e;
				int index;
				
				
				// turn all of the recource tokens into resources
				for(int i=0;i<tasks[task_count].name_value_count;i++){
					// get the index of the : slice part
					e = strchr(task_tokens[task_count][i+4], ':');
					index = (int)(e - task_tokens[task_count][i+4]);
					
					// get the recource name
					strncpy(tasks[task_count].name_value[i].name, task_tokens[task_count][i+4], index);
					strncpy(tasks[task_count].units_held[i].name, task_tokens[task_count][i+4], index);
					// get the resource value
					tasks[task_count].name_value[i].units = task_tokens[task_count][i+4][index+1] - '0';
					tasks[task_count].units_held[i].units = 0;
					// printf("%s, %d \n", tasks[task_count].name_value[i].name, tasks[task_count].name_value[i].units);
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
	
	/*
	for (int i=0;i<task_count;i++){
		printf("%s, %d\n", tasks[i].task_name, tasks[i].name_value_count);
		for(int j=0;j<tasks[i].name_value_count;j++){
			printf("%s, %d \n", tasks[i].name_value[j].name, tasks[i].name_value[j].units);
		}
		printf("\n");
	}
	*/
	
	
	int rval;
	// start the monitor thread
	rval = pthread_create(&monitor_tid, NULL, monitor_thread, (void*)(&monitorTime));
	

	// start all of the tasks
	THREAD_ARG thread_args[task_count+1];
	for (int i=0;i<task_count;i++){
		thread_args[i].index = i;
		thread_args[i].iter = niter;
		tasks[i].status = WAIT;
		tasks[i].wait_time = 0;
		rval = pthread_create(&(tids[i]), NULL, task_thread, (void*)(&thread_args[i]));
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
		printf("	%s: (maxAvail= %d, held= 0)\n", resources[i].name, resources[i].units);
	
	}
	
	// print the end result
	printf("System Tasks: \n");
	for (int i=0;i<task_count;i++){
		printf("[%d] %s (%s, runTime= %d msec, idleTime= %d msec): \n", i+1, tasks[i].task_name, getStatus(tasks[i].status), tasks[i].busy_time, tasks[i].idle_time);
		printf("	(tid= %1lx)\n", tids[i]);
		for(int j= 0;j<tasks[i].name_value_count;j++){
			printf("	%s: (maxAvail= %d, held= %d)\n", tasks[i].name_value[j].name, tasks[i].name_value[j].units, tasks[i].units_held[j].units);
	
		}
		printf("	(RUN: %d times, WAIT: %.2f msec)", niter, tasks[i].wait_time);
		printf("\n");
	}
	
	static long click = 0;
	click = sysconf(_SC_CLK_TCK);
	en_time = times(&en_cpu);
	double cpu_time_used = (double)(en_time - st_time)/click *1000;
	
	printf("Running time= %.2f msec\n", cpu_time_used);
	
	return 0;
}
