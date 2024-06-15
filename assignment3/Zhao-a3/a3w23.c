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
#define NCLIENT 3
#define MAXWORD 80
#define MAX_MSG_LEN 132
#define MAXLINE 128
// ------------------------------


typedef enum {
    PUT,
    GET,
    DELETE,
    GTIME,
    TIME,
    OK,
    ERROR,
    DONE,
    HELLO
} PACKETTYPE;

typedef struct {
    PACKETTYPE type;
    int client_id;
    char message[MAXWORD];
    char file_content[3][80];
    int fileline_count;
} PACKET;

typedef struct sockaddr  SA;
#define MAXBUF  sizeof(PACKET)

void print_packet(PACKET packet) {
// prints the packets
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
        case DONE:
            type_str = "DONE";
            break;
        case HELLO:
            type_str = "HELLO";
            break;
        default:
            type_str = "UNKNOWN";
            break;
    }
    printf("Type: %s\n", type_str);
    printf("Client ID: %d\n", packet.client_id);
    printf("Message: %s\n", packet.message);
}

// ------------------------------
// The WARNING and FATAL functions are due to the authors of
// the AWK Programming Language.

void FATAL (const char *fmt, ... )
{
    va_list  ap;
    fflush (stdout);
    va_start (ap, fmt);  vfprintf (stderr, fmt, ap);  va_end(ap);
    fflush (NULL);
    exit(1);
}

void WARNING (const char *fmt, ... )
{
    va_list  ap;
    fflush (stdout);
    va_start (ap, fmt);  vfprintf (stderr, fmt, ap);  va_end(ap);
}
// ------------------------------------------------------------

int send_packet(int fd, PACKETTYPE type, int id, char *file, int line_count, char content_lines[3][80]){
	/*
	taken inspieration from cmput379 lab
	posted on eclass
	*/
	int len;
	PACKET packet;
	
	assert(fd >= 0);
	
	memset((char *) &packet, 0, sizeof(packet));
	
	packet.type = type;
	strcpy(packet.message, file);
	packet.client_id = id;
	packet.fileline_count = line_count;
	
	// copy all of the lines
	for(int i=0;i<line_count;i++){
		strcpy(packet.file_content[i], content_lines[i]);
	}
	
	// print_packet(packet);
	len = write(fd, (char *) &packet, sizeof(packet));
	if (len != sizeof(packet)){
		printf(" the size if off!\n");
	}
	
	return len;
}

int rcv_packet(int fd, PACKET *packetp){
	/*
	taken inspieration from cmput379 lab
	posted on eclass
	*/
	int len;
	PACKET packet;
	
	assert(fd >= 0);
	memset((char *) &packet, 0, sizeof(packet));
	len = read(fd, (char*) &packet, sizeof(packet));
	*packetp = packet;
	if (len == 0){  printf("rcvFrame: received frame has zero length \n");}
	else if (len != sizeof(packet)){
		printf(" the size if off!\n");
	}
	
	// print_packet(packet);
	return len;
}

int testDone (int done[], int nClient)
{
	int  i, sum= 0;

	if (nClient == 0) return 0;

	for (i= 1; i <= nClient; i++) sum += done[i];
	if (sum == -nClient) return 0;

	for (i= 1; i <= nClient; i++) { if (done[i] == 0) return 0; }
	return 1;
}    
    

// taken inspieration from lab on eclass
// ------------------------------
int clientConnect (const char *serverName, int portNo)
{
	/*
	taken inspieration from cmput379 lab
	posted on eclass
	*/
	int                 sfd;
	struct sockaddr_in  server;
	struct hostent      *hp;                    // host entity

	// lookup the specified host
	//
	hp= gethostbyname(serverName);
	if (hp == (struct hostent *) NULL){
		FATAL("clientConnect: failed gethostbyname '%s'\n", serverName);
	}

	// put the host's address, and type into a socket structure
	//
	memset ((char *) &server, 0, sizeof server);
	memcpy ((char *) &server.sin_addr, hp->h_addr, hp->h_length);
	server.sin_family= AF_INET;
	server.sin_port= htons(portNo);

	// create a socket, and initiate a connection
	//
	if ( (sfd= socket(AF_INET, SOCK_STREAM, 0)) < 0){
		FATAL ("clientConnect: failed to create a socket \n");
	}

	if (connect(sfd, (SA *) &server, sizeof(server)) < 0){
		FATAL ("clientConnect: failed to connect \n");
	}

	return sfd;
}
// ------------------------------
int serverListen (int portNo, int nClient)
{
	/*
	taken inspieration from cmput379 lab
	posted on eclass
	*/

	int                 sfd;
	struct sockaddr_in  sin;

	memset ((char *) &sin, 0, sizeof(sin));

	// create a managing socket
	//
	if ( (sfd= socket (AF_INET, SOCK_STREAM, 0)) < 0){
		FATAL ("serverListen: failed to create a socket \n");
	}

	// bind the managing socket to a name
	sin.sin_family= AF_INET;
	sin.sin_addr.s_addr= htonl(INADDR_ANY);
	sin.sin_port= htons(portNo);

	if (bind (sfd, (SA *) &sin, sizeof(sin)) < 0){
		FATAL ("serverListen: bind failed \n");
	}

	// indicate how many connection requests can be queued
	listen (sfd, nClient+1);
	return sfd;
}

void do_client(int id, char *input, char *server_address, int port_number){
	/*
	do the client loop of the code
	some parts of the code taken inspieration from cmput379 lab
	
	*/
	// initilize the items and connect to the server
	int    len, sfd;
	char   serverName[MAXWORD];
	struct  timespec delay;

	strcpy (serverName, server_address);
	printf ("do_client: trying server '%s', portNo= %d\n", serverName, port_number);
	sfd= clientConnect (serverName, port_number);
	printf ("do_client: connected \n\n");
	
	// send the hello
	char null_file_content[3][80];
	len = send_packet(sfd, HELLO, id, "", 0, null_file_content);
	printf("Transmitted (src= %d) (%s, idNumber= %d) \n", id, "HELLO", id);
	PACKET connect_packet;
	len = rcv_packet(sfd, &connect_packet);
	if (connect_packet.type == ERROR){
		printf("Received (src= 0) (%s: %s)\n", "ERROR", connect_packet.message);}
	else if(connect_packet.type == OK){
		printf("Received (src= 0) %s\n", "OK");
	}
	printf("\n");

	delay.tv_sec=  0;
	delay.tv_nsec= 900E+6;	// 900 millsec (this field can't be 1 second)
	
	FILE *fp;
 	fp = fopen(input, "r");
 	if (fp == NULL){
 		printf("cannot open file!");
 		exit(0);
 	}
	
	char line[MAX_MSG_LEN];
	char token_commands[3][MAXWORD];
	char file_content[3][80];
	PACKETTYPE temp_type;
	bool put_continue = false;
	int fileline_count = 0;
	
	// declare the packet for the use of reciving packet
	PACKET packet;
	
	while(fgets(line, MAX_MSG_LEN, fp)!= NULL){
		// printf("%s\n", line);
		if (line[0] == '\n'){
			// empty line skipped
		}
		else if (line[0] == '#'){
			// comment skipped
		}
		else if (line[0] == '}'){
			// the put command is all inputted now we execute the put command
			if(put_continue == true){
				char temp[MAXWORD];
				strcpy(temp, token_commands[0]);
				int client_id = atoi(temp);
				if(client_id == id){
					temp_type = PUT;
					len = send_packet(sfd, temp_type, client_id, token_commands[2], fileline_count, file_content);
					printf("Transmitted (src= client:%d) (%s: %s) \n", id, "PUT", token_commands[2]);
					for(int i=0;i<fileline_count;i++){
						printf(" [%d]:  '%s'\n", i, file_content[i]);
					}
				}
				
				len = rcv_packet(sfd, &packet);
				if (packet.type == ERROR){
					printf("Received (src= 0) (%s: %s)\n", "ERROR", packet.message);}
				else if(packet.type == OK){
					printf("Received (src= 0) (%s)\n", "OK");}
				
				printf("\n");
				// stop putting items
				put_continue = false;
				// reset the count line
				fileline_count = 0;
			}
		}
		else if (line[0] == '{' ){
			// skip next line is the start of put object
		} 
		else if(put_continue == true){
			// get the lines and then copy it 
			line[strcspn(line, "\n")] = 0;
			strcpy(file_content[fileline_count], line);
			fileline_count += 1;
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
			char temp_content[3][80];
			if(client_id == id){
				
				// printf("%s, %s\n", token_commands[0], token_commands[1]);
				
				if(strcmp(token_commands[1], "gtime") == 0){
					 // printf("it worked!\n");
					temp_type = GTIME;
					char temp[] = "none";
					len = send_packet(sfd, temp_type, client_id, temp, 0, temp_content);
					printf("Transmitted (src= %d) %s \n", id, "GTIME");
				}

				else if(strcmp(token_commands[1], "quit") == 0){
					// exit the program
					temp_type = DONE;
					len = send_packet(sfd, temp_type, client_id, token_commands[2], 0, temp_content);
					printf("Transmitted (src= %d) %s \n", id, "DONE");
					
					len = rcv_packet(sfd, &packet);
					if (packet.type == ERROR){
						printf("Received (src= 0) (%s: %s)\n", "ERROR", packet.message);}
					else if(packet.type == OK){
						printf("Received (src= 0) %s\n", "OK");}
					printf("do_client: client closing connection \n"); close(sfd);
					exit(0);
				}

				else if(strcmp(token_commands[1], "put") == 0){
					// skip this as we gotta continue to get the things for put
					put_continue = true;
				}
				else if(strcmp(token_commands[1], "get") == 0){
					temp_type = GET;
					len = send_packet(sfd, temp_type, client_id, token_commands[2], 0, temp_content);
					printf("Transmitted (src= %d) (%s: %s) \n", id, "GET", token_commands[2]);
				}
				else if(strcmp(token_commands[1], "delete") == 0){
					temp_type = DELETE;
					len = send_packet(sfd, temp_type, client_id, token_commands[2],0, temp_content);
					printf("Transmitted (src= %d) (%s: %s) \n", id, "DELETE", token_commands[2]);
				}
				else if(strcmp(token_commands[1], "delay") == 0){
					printf("*** Enetring a delay peroid of %s msec \n", token_commands[2]);
					int delay_time = atoi(token_commands[2]);
					usleep(delay_time * 1000);
					printf("*** Exiting delay peroid \n \n");
				}	
				
				
				if(strcmp(token_commands[1], "delay") != 0  && put_continue == false){
					// wait and recive the message from the server
					len = rcv_packet(sfd, &packet);
					if (packet.type == ERROR){
						printf("Received (src= 0) (%s: %s)\n", "ERROR", packet.message);
						for(int i=0;i<packet.fileline_count;i++){
							printf(" [%d]:  '%s'\n", i, packet.file_content[i]);
						}
					}
					else if(packet.type == OK){
						printf("Received (src= 0) %s\n", "OK");
						for(int i=0;i<packet.fileline_count;i++){
							printf(" [%d]:  '%s'\n", i, packet.file_content[i]);
						}
					}
					else if(packet.type == TIME){
						printf("Received (src= 0) (%s: %s)\n", "TIME", packet.message);
					}
						
					printf("\n");
				}
			}
		}
	}
	close(sfd);
}

void do_server(int port_number){
	// run the server
	// The done[] flags:
	// 	done[i]= -1 when client i has not connected
	// 	=  0 when client i has connected but not finished
	//  	= +1 when client i has finished or server lost connection
	// some parts of the code taken inspieration from cmput379 lab
	int   i, N, len, rval, timeout, done[NCLIENT+1];
	int   newsock[NCLIENT+1];
	char  buf[MAXBUF], line[MAXLINE];
	// calculate time
	clock_t st_time;
	clock_t en_time;
	struct tms st_cpu;
	struct tms en_cpu;
	
	st_time = times(&st_cpu);
	
	char data[NCLIENT][32][MAXWORD];
	int data_line_counter[NCLIENT][MAXWORD];
	// there is max 3 lines
	char data_lines[NCLIENT][32][4][MAXWORD];
	
	int data_counter[NCLIENT];
	data_counter[0] = 0;
	data_counter[1] = 0;
	data_counter[2] = 0;

	struct pollfd       pfd[NCLIENT+2];
	struct sockaddr_in  from;
	socklen_t           fromlen;

	for (i= 0; i <= NCLIENT; i++) done[i]= -1;

	// prepare for noblocking I/O polling from the managing socket
	timeout= 0;
	pfd[0].fd=  serverListen (port_number, NCLIENT);;
	pfd[0].events= POLLIN;
	pfd[0].revents= 0;
	
	pfd[1].fd= STDIN_FILENO;
	pfd[1].events= POLLIN;

	printf ("Server is accepting connections (port= %d)\n\n", port_number);

	N= 2;		// N descriptors to poll
	PACKET temp_packet;
	timeout = 10000;
	char nullList[3][80];

	while(1){
		rval= poll (pfd, N, timeout);
		if ((N < NCLIENT+2) && (pfd[0].revents & POLLIN) ) {
		   // accept a new connection
		   fromlen= sizeof(from);
		   newsock[N]= accept(pfd[0].fd, (SA *) &from, &fromlen);

		   pfd[N].fd= newsock[N];
		   pfd[N].events= POLLIN;
		   pfd[N].revents= 0;
		   done[N]= 0;
		   N++;
		}
		
		for(int i = 2; i <= N-1; i++){
			// proccess all of the clients stuff
			if((done[i] == 0) && (pfd[i].revents & POLLIN)){
				len = rcv_packet(pfd[i].fd, &temp_packet);
				
				// check if the client has lost connection or not
				if (len == 0) {
					printf ("server lost connection with client\n\n");
					done[i]= 1;
					continue;      // start a new for iteration
				}
				
				if(temp_packet.type == HELLO){
					// get the hello from the client
					printf("Received (src= %d) (%s, idNumber= %d)\n", i-1, "HELLO", i-1);
					len = send_packet(pfd[i].fd, OK, i-1, "", 0, nullList);
					printf("Transmitted (src= 0) OK \n\n");
				}
				
				else if(temp_packet.type == PUT){
					printf("Received (src= %d) (%s: %s)\n", i-1, "PUT", temp_packet.message);
					for(int k=0;k<temp_packet.fileline_count;k++){
						printf(" [%d]:  '%s' \n", k, temp_packet.file_content[k]);
					}
					// use the same thing to find the file to get as delete
					char stringToDelete[MAXWORD];
					strcpy(stringToDelete, temp_packet.message);

					// Find the index of the string to delete
					int iToDelete = i-2, jToDelete = -1;
					for (int j = 0; j < 32; j++) {
						if (strcmp(data[iToDelete][j], stringToDelete) == 0) {
						    jToDelete = j;
						    break;
						}
					}

					if (jToDelete != -1) {
						len = send_packet(pfd[i].fd, ERROR, i-1, "Object already exits", 0, nullList);
						printf("Transmitted (src= 0) (ERROR: Object already exits) \n");
					}
					else{
						// copy the recoved data to the data list
						strcpy(data[i-2][data_counter[i-2]], temp_packet.message);
						for(int m=0;m<temp_packet.fileline_count;m++){
							strcpy(data_lines[iToDelete][data_counter[i-2]][m], temp_packet.file_content[m]);
						}
						data_line_counter[i-2][data_counter[i-2]] = temp_packet.fileline_count;
						data_counter[i-2]++;
						len = send_packet(pfd[i].fd, OK, i-1, "", 0, nullList);
						printf("Transmitted (src= 0) OK \n\n");
					}
				}
				
				else if(temp_packet.type == GET){
					printf("Received (src= %d) (%s: %s)\n", i-1, "GET", temp_packet.message);
					// use the same thing to find the file to get as delete
					char stringToDelete[MAXWORD];
					strcpy(stringToDelete, temp_packet.message);

					// Find the index of the string to delete
					int iToDelete = -1, jToDelete = -1;
					for(int k = 0; k < 3; k++){
						for (int j = 0; j < 32; j++) {
							if (strcmp(data[k][j], stringToDelete) == 0) {
							    iToDelete = k;
							    jToDelete = j;
							    break;
							}
						}
					}

					if (jToDelete == -1 || iToDelete == -1) {
						char temp_object_not_found[3][80];
						strcpy(temp_object_not_found[0], "Object not found");
						len = send_packet(pfd[i].fd, ERROR, i, "Object not found", 1, temp_object_not_found);
						printf("Transmitted (src= 0) (ERROR: Object not found) \n\n");
					}
					else{
						char temp_object_found[3][80];
						for(int m=0;m<data_line_counter[iToDelete][jToDelete];m++){
							strcpy(temp_object_found[m], data_lines[iToDelete][jToDelete][m]);
						}
						// there is the file get succesful
						len = send_packet(pfd[i].fd, OK, i-1, "", data_line_counter[iToDelete][jToDelete], temp_object_found);
						printf("Transmitted (src= 0) OK \n\n");
					}
				}
				
				else if(temp_packet.type == DELETE){
					printf("Received (src= %d) (%s: %s)\n", i-1, "DELETE", temp_packet.message);
					char stringToDelete[MAXWORD];
					strcpy(stringToDelete, temp_packet.message);

					// Find the index of the string to delete
					int iToDelete = i-2, jToDelete = -1;
					for (int j = 0; j < 32; j++) {
						if (strcmp(data[iToDelete][j], stringToDelete) == 0) {
						    jToDelete = j;
						    break;
						}
					}

					if (jToDelete == -1) {
						len = send_packet(pfd[i].fd, ERROR, i-1, "client not owner", 0, nullList);
						printf("Transmitted (src= 0) (ERROR: client not owner) \n\n");
					}
					else{
						// Shift the remaining elements to fill the gap left by the deleted string
						for (int j = jToDelete; j < data_counter[i-2]; j++) {
							strcpy(data[iToDelete][j], data[iToDelete][j+1]);
							// move the data counter
							data_line_counter[iToDelete][j]= data_line_counter[iToDelete][j+1];
							// move the next message to the spot
							for(int k = 0; k < 3; k++){
								strcpy(data_lines[iToDelete][j][k], data_lines[iToDelete][j+1][k]);
							}
						}

						// Set the last element to an empty string
						data_counter[i-2] = data_counter[i-2] -1;
						data_line_counter[iToDelete][data_counter[i-2]]= 0;
						strcpy(data[iToDelete][data_counter[i-2]], "");
						// move the rest of the space should be ""
						for(int k = 0; k < 3; k++){
							strcpy(data_lines[iToDelete][data_counter[i-2]][k], "");
						}
						
						len = send_packet(pfd[i].fd, OK, i-1, "", 0, nullList);
						printf("Transmitted (src= 0) OK \n\n");
					}
				}
				
				else if(temp_packet.type == GTIME){
					printf("Received (src= %d) (%s)\n", i-1, "GTIME");
					static long click = 0;
					click = sysconf(_SC_CLK_TCK);
					en_time = times(&en_cpu);
					double cpu_time_used = (double)(en_time - st_time)/click;
					char str[20];
					sprintf(str, "%.2f", cpu_time_used);
					len = send_packet(pfd[i].fd, TIME, i-1, str, 0, nullList);
					printf("Transmitted (src= 0) (TIME:     %s) \n\n", str);
				}
				
				else if(temp_packet.type == DONE){
					printf("Received (src= %d) %s\n", i-1, "DONE");
					// make one of the client done
					done[i]= 1;
					len = send_packet(pfd[i].fd, OK, i-1, "", 0, nullList);
					printf("Transmitted (src= 0) %s) \n\n", "OK");
				}
			}
		}
		
		// the last one is for the server input commands
		if(pfd[1].revents & POLLIN){
			char cmd[MAX_MSG_LEN];
			
			fgets(cmd, sizeof(cmd), stdin);
			
			if(strcmp(cmd, "list\n") == 0){
				printf("object table: \n");
				for(int i = 0; i < 3; i++){
					for(int j = 0; j < data_counter[i]; j++){
						printf("(owner: %d, name = %s)\n", i+1, data[i][j]);
						
						// print the line content being stored
						for(int m=0;m<data_line_counter[i][j];m++){
							printf("[%d]: '%s' \n", m, data_lines[i][j][m]);
						}
					}
				}
				printf("\n");
			}
			else if(strcmp(cmd, "quit\n") == 0){
				printf("quitting!\n");
				printf ("do_server: server closing main socket (");
				for (i=2 ; i <= N-1; i++) printf ("done[%d]= %d, ", i, done[i]);
				printf (") \n");
				for (i=2; i <= N-1; i++) close(pfd[i].fd);
				close(pfd[0].fd);
				exit(0);
			}
		}
		if(testDone(done, N-1) == 1){
			// print all of the objects before breaking
			printf("object table: \n");
			for(int i = 0; i < 3; i++){
				for(int j = 0; j < data_counter[i]; j++){
					printf("(owner: %d, name = %s)\n", i+1, data[i][j]);
					
					// print the line content being stored
					for(int m=0;m<data_line_counter[i][j];m++){
						printf("[%d]: '%s' \n", m, data_lines[i][j][m]);
					}
				}
			}
			printf("\n");
			break;	
		 
		}
	}
	printf ("do_server: server closing main socket (");
	for (i=2 ; i <= N-1; i++) printf ("done[%d]= %d, ", i, done[i]);
	printf (") \n");
	for (i=2; i <= N-1; i++) close(pfd[i].fd);
	close(pfd[0].fd);
}

int main(int argc, char *argv[]) {
	//printf("it works here");
	if (argc < 2){ printf("Incorrect Usage!"); exit(0);}
	
	if (strcmp(argv[1], "-c") == 0){
		// printf("it works here!");
		int client_id = atoi(argv[2]);
		char *inputfile = argv[3];
		char *serverAddress = argv[4];
		int portNumber = atoi(argv[5]);
		printf("main: do_client (idNumber= %d, inputFile= '%s') \n", client_id, argv[3]);
		printf("\t (server= '%s', port= %d)\n", argv[4], portNumber);
		do_client(client_id, inputfile, serverAddress, portNumber);
	}
	
	if (strcmp(argv[1], "-s") == 0){
		printf("a3w23: do_server\n");
		int port_number = atoi(argv[2]);
		do_server(port_number);
	}
	
	return 0;
}
