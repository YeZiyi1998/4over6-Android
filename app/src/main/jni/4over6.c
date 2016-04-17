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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
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
int vpn_handle;
int fifo_handle_stats;

boolean isClosed = false;

pthread_mutex_t traffic_mutex_in;
pthread_mutex_t traffic_mutex_out;

void timer() {
	// Initialize Heart beat Message
	Message heart_beat;
	bzero(&heart_beat, sizeof(Message));
	heart_beat.length = sizeof(Message);
	heart_beat.type = MSGTYPE_HEARTBEAT;
	char buffer[MAX_BUFFER+1];
	char fifo_buffer[MAX_BUFFER+1];
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
		// Send Stats
		bzero(fifo_buffer, MAX_BUFFER+1);
		sprintf(fifo_buffer, "%d %d %d %d", out_length, out_times, in_length, in_times);
		CHK(write(fifo_handle_stats, fifo_buffer, strlen(fifo_buffer) + 1));

		// Clear Stats
		pthread_mutex_lock(&traffic_mutex_out);
		out_length = 0;
		out_times = 0;
		pthread_mutex_unlock(&traffic_mutex_out);
		pthread_mutex_lock(&traffic_mutex_in);
		in_length = 0;
		in_times = 0;
		pthread_mutex_unlock(&traffic_mutex_in);
		sleep(1);
	}
}

void vpnService() {
	char buffer[MAX_BUFFER+1];
	bzero(buffer, MAX_BUFFER+1);
	int len;
	Message msg;
	bzero(&msg, sizeof(Message));
	while((len = read(vpn_handle, buffer, MAX_BUFFER)) != -1) {
		msg.length = len + 5;
		msg.type = MSGTYPE_DATA_SEND;
		memcpy(msg.data, buffer, len);
		memcpy(buffer, &msg, sizeof(Message));

		// Send to server
		len = send(client_socket, buffer, sizeof(Message), 0);
		if(len != sizeof(Message)) {
			printf("Send Error!\n");
		}

		// Do stats
		pthread_mutex_lock(&traffic_mutex_out);
		out_length += msg.length - 5;
		out_times += 1;
		pthread_mutex_unlock(&traffic_mutex_out);

		bzero(&msg, sizeof(Message));
		bzero(buffer, MAX_BUFFER+1);
	}
	printf("vpn_handle read failed!\n");
}

int main(void) {
	char buffer[MAX_BUFFER+1];
	bzero(buffer, MAX_BUFFER+1);

	int fifo_handle;
	char * fifo_name = "/tmp/myfifo";
	char * fifo_name_stats = "/tmp/myfifo_stats";
	/* create the FIFO (named pipe) */
	mkfifo(fifo_name, 0666);
	mkfifo(fifo_name_stats, 0666);
	CHK(fifo_handle = open(fifo_name, O_RDWR|O_CREAT|O_TRUNC));
	CHK(fifo_handle_stats = open(fifo_name_stats, O_RDWR|O_CREAT|O_TRUNC));

	pthread_mutex_init(&traffic_mutex_in, NULL);
	pthread_mutex_init(&traffic_mutex_out, NULL);

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
			printf("Type: IP_REC\nContents: %s\n", msg.data);
			len = strlen(msg.data) + 1;
			int size = write(fifo_handle, msg.data, strlen(msg.data)+1);
			if(len != size) {
				fprintf( stderr, "(line %d): ERROR - %s.\n", __LINE__, strerror( errno ) );
				exit(1);
			}

			// Now Wait for File Handle
			memset(buffer, 0, MAX_BUFFER + 1);
			len = read(fifo_handle, buffer, MAX_BUFFER);
			if(len != sizeof(int)) {
				printf("File Handle Read Error! Read %s (len:%d)\n", buffer, len);
				exit(1);
			}
			vpn_handle = *(int*)buffer;
			printf("Get VPN Handle %d Succeeded!\n", vpn_handle);
			// Create a new thread for VPN service
			pthread_t vpn_thread;
			pthread_create(&vpn_thread, NULL, vpnService, NULL);
		}
		else if(msg.type == MSGTYPE_DATA_RECV) {
			printf("Type: DATA_REC (length: %d)\nContents: %s\n", msg.length, msg.data);
			len = write(vpn_handle, msg.data, msg.length-5);
			if (len != msg.length-5) {
				printf("Error!\n");
			}

			// Do Stats
			pthread_mutex_lock(&traffic_mutex_in);
			in_times ++;
			in_length += len;
			pthread_mutex_unlock(&traffic_mutex_in);
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
