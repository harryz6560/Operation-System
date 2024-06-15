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



#define NCLIENT 3
#define MAXWORD 32
#define MAX_MSG_LEN 132

int fifocs[3];
int fifosc[3];
char fifo_names[3][2][10] = {{"fifo-1-0", "fifo-0-1"},
{"fifo-2-0", "fifo-0-2"},{"fifo-3-0", "fifo-0-3"}};

typedef enum {
    PUT,
    GET,
    DELETE,
    GTIME,
    TIME,
    OK,
    ERROR,
    QUIT,
} PACKETTYPE;

typedef struct {
    PACKETTYPE type;
    int client_id;
    char message[MAXWORD];
} PACKET;

void print_packet(PACKET packet) {
    const char* type_str;
    switch (packet.type) {
        case PUT:
            type_str = "PUT";
            break;
        case GET:
            type_str = "GET";
            break;
        case DELETE:
            type_str = "DELETE";
            break;
        case GTIME:
            type_str = "GTIME";
            break;
        case TIME:
            type_str = "TIME";
            break;
        case OK:
            type_str = "OK";
            break;
        case ERROR:
            type_str = "ERROR";
            break;
        case QUIT:
            type_str = "QUIT";
            break;
        default:
            type_str = "UNKNOWN";
            break;
    }
    printf("Type: %s\n", type_str);
    printf("Client ID: %d\n", packet.client_id);
    printf("Message: %s\n", packet.message);
}

void send_packet(int fd, PACKETTYPE type, int id, char *file){
	/*
	taken inspieration from cmput379 lab
	posted on eclass
	*/
	PACKET packet;
	
	assert(fd >= 0);
	
	memset((char *) &packet, 0, sizeof(packet));
	
	packet.type = type;
	strcpy(packet.message, file);
	packet.client_id = id;
	
	// print_packet(packet);
	write(fd, (char *) &packet, sizeof(packet));
}

PACKET rcv_packet(int fd){
	/*
	taken inspieration from cmput379 lab
	posted on eclass
	*/
	int len;
	PACKET packet;
	
	assert(fd >= 0);
	memset((char *) &packet, 0, sizeof(packet));
	len = read(fd, (char*) &packet, sizeof(packet));
	if (len != sizeof(packet)){
		printf(" the size if off!\n");
	}
	
	// print_packet(packet);
	return packet;
}

void do_client(int id, char *input){
	// runs the client part of the program
	int fifoCS, fifoSC;
	
	// printf("is been succesful!!");
	
	if ( (fifoCS= open(fifo_names[id-1][0], O_RDWR)) < 0){
		printf("cannot open file!%s", fifo_names[id-1][0]);
		exit(0);}
	
	if ( (fifoSC= open(fifo_names[id-1][1], O_RDWR)) < 0){
		printf("cannot open file!%s", fifo_names[id-1][1]);
		exit(0);}
	
	FILE *fp;
 	fp = fopen(input, "r");
 	if (fp == NULL){
 		printf("cannot open file!");
 		exit(0);
 	}
	
	char line[MAX_MSG_LEN];
	char token_commands[3][MAXWORD];
	PACKET packet;
	PACKETTYPE temp_type;
	
	while(fgets(line, MAX_MSG_LEN, fp)!= NULL){
		// printf("%s\n", line);
		if (line[0] == '\n'){
			// empty line skipped
		}
		else if (line[0] == '#'){
			// comment skipped
		}
		else if (line[0] == '\r'){
			// skip the next time
		}
		else{
			// tokenize the line
            		line[strlen(line)-1] = '\0';
			// copy the command    
			// token these commands
			// token variables
			char *token;
			int count = 0;
			char test_str[MAX_MSG_LEN];
			strcpy(test_str, line);

			token = strtok(test_str, " \r\n"); // the first token
			while(token != NULL){
			strcpy(token_commands[count], token);
			count = count+1;
			token = strtok(NULL, " \r\n");
			}

			char temp[MAXWORD];
			strcpy(temp, token_commands[0]);
			int client_id = atoi(temp);
			if(client_id == id){
				
				// printf("%s, %s\n", token_commands[0], token_commands[1]);
				
				if(strcmp(token_commands[1], "gtime") == 0){
					 // printf("it worked!\n");
					temp_type = GTIME;
					char temp[] = "none";
					send_packet(fifoCS, temp_type, client_id, temp);
					printf("Transmitted (src= client:%d) %s \n", id, "GTIME");
				}

				else if(strcmp(token_commands[1], "quit") == 0){
					// exit the program
					fclose(fp);
					close(fifoCS);
					exit(0);
				}

				else if(strcmp(token_commands[1], "put") == 0){
					temp_type = PUT;
					send_packet(fifoCS, temp_type, client_id, token_commands[2]);
					printf("Transmitted (src= client:%d) (%s: %s) \n", id, "PUT", token_commands[2]);
				}
				else if(strcmp(token_commands[1], "get") == 0){
					temp_type = GET;
					send_packet(fifoCS, temp_type, client_id, token_commands[2]);
					printf("Transmitted (src= client:%d) (%s: %s) \n", id, "GET", token_commands[2]);
				}
				else if(strcmp(token_commands[1], "delete") == 0){
					temp_type = DELETE;
					send_packet(fifoCS, temp_type, client_id, token_commands[2]);
					printf("Transmitted (src= client:%d) (%s: %s) \n", id, "DELETE", token_commands[2]);
				}
				else if(strcmp(token_commands[1], "delay") == 0){
					printf("*** Enetring a delay peroid of %s msec \n", token_commands[2]);
					int delay_time = atoi(token_commands[2]);
					usleep(delay_time * 1000);
					printf("*** Exiting delay peroid \n \n");
				}	
				
				
				if(strcmp(token_commands[1], "delay") != 0){
					// wait and recive the message from the server
					packet = rcv_packet(fifoSC);
					if (packet.type == ERROR){
						printf("Received (src= server) (%s: %s)\n\n", "ERROR", packet.message);}
					else if(packet.type == OK){
						printf("Received (src= server) (%s: %s)\n\n", "OK", packet.message);}
					else if(packet.type == TIME){
						printf("Received (src= server) (%s: %s)\n\n", "TIME", packet.message);}
				}
			}
		}
	}
	
	fclose(fp);
}

void do_server(){
	// runs the server part of the program
	// printf("it works here");
	// calculate time
	clock_t st_time;
	clock_t en_time;
	struct tms st_cpu;
	struct tms en_cpu;
	
	st_time = times(&st_cpu);
	
	char data[NCLIENT][MAXWORD][MAXWORD];
	struct pollfd fds[5];
	
	int data_counter[NCLIENT];
	data_counter[0] = 0;
	data_counter[1] = 0;
	data_counter[2] = 0;
	
	for(int k = 0; k < 3; k++){
		if ( (fifocs[k]= open(fifo_names[k][0], O_RDWR)) < 0){
			printf("cannot open file!%s", fifo_names[k][0]);
			exit(0);}
		// printf("%d, %s\n", fifocs[k],fifo_names[k][0]);
		if ( (fifosc[k]= open(fifo_names[k][1], O_RDWR)) < 0){
			printf("cannot open file!%s", fifo_names[k][1]);
			exit(0);}
		// printf("%d, %s\n", fifosc[k],fifo_names[k][1]);
	}

	
	// set all of the fds for the file
	fds[0].fd = fifocs[0];
	fds[0].events = POLLIN;
	fds[1].fd = fifocs[1];
	fds[1].events = POLLIN;
	fds[2].fd = fifocs[2];
	fds[2].events = POLLIN;
	fds[3].fd = STDIN_FILENO;
	fds[3].events = POLLIN;
	
	PACKET temp_packet;
	int timeout = 10000;
	

	while(1){
		int ret = poll(fds, 4, timeout);
		
		if (ret == -1){
			printf("poll error\n");
			exit(0);
		}
		else{
			for(int i = 0; i < 3; i++){
				// proccess all of the clients stuff
				if(fds[i].revents & POLLIN){
					temp_packet = rcv_packet(fds[i].fd);

					if(temp_packet.type == PUT){
						printf("Received (src= client: %d) (%s: %s)\n", i+1, "PUT", temp_packet.message);
						// use the same thing to find the file to get as delete
						char stringToDelete[MAXWORD];
						strcpy(stringToDelete, temp_packet.message);

						// Find the index of the string to delete
						int iToDelete = i, jToDelete = -1;
						for (int j = 0; j < MAXWORD; j++) {
							if (strcmp(data[iToDelete][j], stringToDelete) == 0) {
							    jToDelete = j;
							    break;
							}
						}

						if (jToDelete != -1) {
							send_packet(fifosc[i], ERROR, i+1, "Object already exits");
							printf("Transmitted (src= server) (ERROR: Object already exits) \n");
						}
						else{
							// copy the recoved data to the data list
							strcpy(data[i][data_counter[i]], temp_packet.message);
							data_counter[i]++;
							send_packet(fifosc[i], OK, i+1, "");
							printf("Transmitted (src= server) OK \n\n");
						}
					}
					
					else if(temp_packet.type == GET){
						printf("Received (src= client: %d) (%s: %s)\n", i+1, "GET", temp_packet.message);
						// use the same thing to find the file to get as delete
						char stringToDelete[MAXWORD];
						strcpy(stringToDelete, temp_packet.message);

						// Find the index of the string to delete
						int iToDelete = -1, jToDelete = -1;
						for(int k = 0; k < 3; k++){
							for (int j = 0; j < MAXWORD; j++) {
								if (strcmp(data[k][j], stringToDelete) == 0) {
								    iToDelete = k;
								    jToDelete = j;
								    break;
								}
							}
						}

						if (jToDelete == -1 || iToDelete == -1) {
							send_packet(fifosc[i], ERROR, i+1, "Object not found");
							printf("Transmitted (src= server) (ERROR: Object not found) \n\n");
						}
						else{
							// there is the file get succesful
							send_packet(fifosc[i], OK, i+1, "");
							printf("Transmitted (src= server) OK \n\n");
						}
					}
					
					else if(temp_packet.type == DELETE){
						printf("Received (src= client: %d) (%s: %s)\n", i+1, "DELETE", temp_packet.message);
						char stringToDelete[MAXWORD];
						strcpy(stringToDelete, temp_packet.message);

						// Find the index of the string to delete
						int iToDelete = i, jToDelete = -1;
						for (int j = 0; j < MAXWORD; j++) {
							if (strcmp(data[iToDelete][j], stringToDelete) == 0) {
							    jToDelete = j;
							    break;
							}
						}

						if (jToDelete == -1) {
							send_packet(fifosc[i], ERROR, i+1, "client not owner");
							printf("Transmitted (src= server) (ERROR: client not owner) \n\n");
						}
						else{
							// Shift the remaining elements to fill the gap left by the deleted string
							for (int j = jToDelete; j < data_counter[i]; j++) {
								strcpy(data[iToDelete][j], data[iToDelete][j+1]);
							}

							// Set the last element to an empty string
							data_counter[i] = data_counter[i] -1;
							strcpy(data[iToDelete][data_counter[i]], "");
							send_packet(fifosc[i], OK, i+1, "");
							printf("Transmitted (src= server) OK \n\n");
						}
					}
					
					else if(temp_packet.type == GTIME){
						printf("Received (src= client: %d) (%s)\n", i+1, "GTIME");
						static long click = 0;
						click = sysconf(_SC_CLK_TCK);
						en_time = times(&en_cpu);
						double cpu_time_used = (double)(en_time - st_time)/click;
						char str[20];
						sprintf(str, "%.2f", cpu_time_used);
						send_packet(fifosc[i], TIME, i+1, str);
						printf("Transmitted (src= server) (TIME:     %s) \n\n", str);
					}
				}
			}
			
			// the last one is for the server input commands
			if(fds[3].revents & POLLIN){
				char cmd[MAX_MSG_LEN];
				
				fgets(cmd, sizeof(cmd), stdin);
				
				if(strcmp(cmd, "list\n") == 0){
					printf("object table: \n");
					for(int i = 0; i < 3; i++){
						for(int j = 0; j < data_counter[i]; j++){
							printf("(owner: %d, name = %s)\n", i+1, data[i][j]);
						}
					}
					printf("\n");
				}
				else if(strcmp(cmd, "quit\n") == 0){
					printf("quitting!\n");
					// close the files
					for(int i = 1; i < 4; i++){
						close(fifocs[i]);
						close(fifosc[i]);
					}
					exit(0);
				}
			}	
		}
	}
}

int main(int argc, char *argv[]) {
	//printf("it works here");
	if (argc < 2){ printf("Incorrect Usage!"); exit(0);}
	
	if (strcmp(argv[1], "-c") == 0){
		// printf("it works here!");
		int client_id = atoi(argv[2]);
		char *inputfile = argv[3];
		printf("main: do_client (idNumber= %d, inputFile= %s) \n\n", client_id, argv[3]);
		do_client(client_id, inputfile);
	}
	if (strcmp(argv[1], "-s") == 0){
		printf("a2p2: do_server\n\n");
		do_server();
	}
	
	// close the files
	for(int i = 1; i < 4; i++){
		close(fifocs[i]);
		close(fifosc[i]);
	}
	return 0;
}
