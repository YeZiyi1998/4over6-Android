/*
 ============================================================================
 Name        : 4over6.c
 Author      : MaYe
 Version     : 0.1
 Copyright   : Your copyright notice
 Description : IPv4 over IPv6 client in C
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef enum { false = 0, true } boolean;

typedef struct {
	int length;
	char type;
	char data[4096];
} Message;

#define SERVER_ADDR "2402:f000:1:4417::900"
#define SERVER_PORT 5678
#define MAX_BUFFER 1024

#define MSGTYPE_IP_REQ 100
#define MSGTYPE_IP_REC 101
#define MSGTYPE_DATA_SEND 102
#define MSGTYPE_DATA_RECV 103
#define MSGTYPE_HEARTBEAT 104

#define CHK(expr)  do { if ( (expr) == -1 ) { fprintf( stderr, "(line %d): ERROR - %s.\n", __LINE__, strerror( errno ) ); exit( 1 );  } } while ( false )

int heartbeat_send_counter = 0;
int heartbeat_recv_time = 0;
int out_length = 0;
int out_times = 0;
int in_length = 0;
int in_times = 0;

int client_socket;

boolean isClosed = false;

pthread_mutex_t traffic_mutex;

void timer() {
	// Initialize Heart beat Message
	Message heart_beat;
	bzero(&heart_beat, sizeof(Message));
	heart_beat.length = sizeof(Message);
	heart_beat.type = MSGTYPE_HEARTBEAT;
	char buffer[MAX_BUFFER+1];
	bzero(buffer, MAX_BUFFER+1);
	memcpy(buffer, &heart_beat, sizeof(Message));

	while(1) {
		// First Checkout Heart Beat Time
		int current_time = time((time_t*)NULL);
		if(current_time - heartbeat_recv_time >= 60) {
			// We Lost Server
			isClosed = true;
			// TODO: Change this
			printf("We Lost Server!!!\n");
			exit(1);
		}
		if(heartbeat_send_counter == 0) {
			// Send Heart Beat Package
			int len = send(client_socket, buffer, 5, 0);
			CHK(len);
			if(len != 5) {
				printf("Something happened while sending heart beats!\n");
			}
			heartbeat_send_counter = 19;
		}
		else {
			heartbeat_send_counter--;
		}
		sleep(1);
	}
}

int main(void) {
	char buffer[MAX_BUFFER+1];
	bzero(buffer, MAX_BUFFER+1);

	struct sockaddr_in6 server_socket;
	CHK(client_socket = socket(AF_INET6, SOCK_STREAM, 0));
	printf("socket created!\n");

	bzero(&server_socket, sizeof(server_socket));
	server_socket.sin6_family = AF_INET6;
	server_socket.sin6_port = htons(SERVER_PORT);
	CHK(inet_pton(AF_INET6, SERVER_ADDR, &server_socket.sin6_addr));
	printf("address created!\n");

	CHK(connect(client_socket, (struct sockaddr *) &server_socket, sizeof(server_socket)));
	printf("Connect Succeeded!\n");

	// Initialize Receive Time
	heartbeat_recv_time = time((time_t*)NULL);
	pthread_t timer_thread;
	pthread_create(&timer_thread, NULL, timer, NULL);

	// Now send IP Request
	Message msg;
	bzero(&msg, sizeof(Message));
	msg.length = sizeof(Message);
	msg.type = MSGTYPE_IP_REQ;
	memcpy(buffer, &msg, sizeof(Message));
	CHK(send(client_socket, buffer, sizeof(Message), 0));
	int len;
	while(!isClosed) {
		// Now Receive Package
		bzero(buffer, MAX_BUFFER + 1);
		CHK(len = recv(client_socket, buffer, MAX_BUFFER, 0));
		printf("Receive %d Bytes From Server!\n", len);

		// Now Parse Package
		bzero(&msg, sizeof(Message));
		memcpy(&msg, buffer, sizeof(Message));
		if(msg.type == MSGTYPE_IP_REC) {
			// TODO:
			printf("Type: IP_REC\nContents: %s\n", msg.data);
		}
		else if(msg.type == MSGTYPE_DATA_RECV) {
			printf("Type: DATA_REC\nContents: %s\n", msg.data);
		}
		else if(msg.type == MSGTYPE_HEARTBEAT) {
			printf("Type: HEARTBEAT\nContents: %s\n", msg.data);
			heartbeat_recv_time = time((time_t*)NULL);
		}
		else {
			printf("Unknown Receive Type %d!\n", msg.type);
			printf("Contents: %s\n", msg.data);
		}
	}
	return EXIT_SUCCESS;
}
