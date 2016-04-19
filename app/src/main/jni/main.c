#include "com_example_maye_IVI_MainActivity.h"
#include<android/log.h>

#define TAG "JNI" // 这个是自定义的LOG的标识
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG ,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,TAG ,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,TAG ,__VA_ARGS__)

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
#define MAX_BUFFER 4104

#define MSGTYPE_IP_REQ 100
#define MSGTYPE_IP_REC 101
#define MSGTYPE_DATA_SEND 102
#define MSGTYPE_DATA_RECV 103
#define MSGTYPE_HEARTBEAT 104

#define SERV_BUFFER 240*1024

#define CHK(expr)  do { if ( (expr) == -1 ) { LOGF("(line %d): ERROR - %s.\n", __LINE__, strerror( errno ) ); exit( 1 );  } } while ( false )

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
boolean hasIP = false;

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
			// isClosed = true;
			// TODO: Change this
			LOGF("We Lost Server!!!\n");
			exit(1);
		}
		if(heartbeat_send_counter == 0) {
			// Send Heart Beat Package
			int len = send(client_socket, buffer, 5, 0);
			CHK(len);
			if(len != 5) {
				LOGF("Something happened while sending heart beats!\n");
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
	int len, maxfdp = vpn_handle + 1;
	Message msg;
	bzero(&msg, sizeof(Message));
	fd_set fds;

	while(1) {
		FD_ZERO(&fds);
		FD_SET(vpn_handle ,&fds);
		switch (select(maxfdp, &fds, NULL, NULL, NULL)) {
			case -1:
				LOGF("(line %d): ERROR - %s.\n", __LINE__, strerror( errno ) );
				exit(1);
			case 0:
				break;
			default:
				if(FD_ISSET(vpn_handle, &fds)) {
					CHK(len = read(vpn_handle, buffer, MAX_BUFFER));
					LOGE("Send %d Bytes to Server", len);
					msg.length = len + 5;
					msg.type = MSGTYPE_DATA_SEND;
					memcpy(msg.data, buffer, len);
					memcpy(buffer, &msg, sizeof(Message));

					// Send to server
					len = send(client_socket, buffer, sizeof(Message), 0);
					if(len != sizeof(Message)) {
						LOGD("Send Error!\n");
					}

					// Do stats
					pthread_mutex_lock(&traffic_mutex_out);
					out_length += msg.length - 5;
					out_times += 1;
					pthread_mutex_unlock(&traffic_mutex_out);

					bzero(&msg, sizeof(Message));
					bzero(buffer, MAX_BUFFER+1);
				}
		}
	}
	LOGD("vpn_handle read failed! ERROR %d - %s.\n", errno, strerror( errno ));
}

int main(void) {
	char buffer[MAX_BUFFER+1];
	bzero(buffer, MAX_BUFFER+1);

	int fifo_handle;
	char * fifo_name = "/data/data/com.example.maye.IVI/myfifo";
	char * fifo_name_stats = "/data/data/com.example.maye.IVI/myfifo_stats";
	/* create the FIFO (named pipe) */
	mkfifo(fifo_name, 0666);
	mkfifo(fifo_name_stats, 0666);
	CHK(fifo_handle = open(fifo_name, O_RDWR|O_CREAT|O_TRUNC));
	CHK(fifo_handle_stats = open(fifo_name_stats, O_RDWR|O_CREAT|O_TRUNC));

	pthread_mutex_init(&traffic_mutex_in, NULL);
	pthread_mutex_init(&traffic_mutex_out, NULL);

	struct sockaddr_in6 server_socket;
	CHK(client_socket = socket(AF_INET6, SOCK_STREAM, 0));
	int bufferSize = SERV_BUFFER;
	//setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));
	LOGD("socket created!\n");

	bzero(&server_socket, sizeof(server_socket));
	server_socket.sin6_family = AF_INET6;
	server_socket.sin6_port = htons(SERVER_PORT);
	CHK(inet_pton(AF_INET6, SERVER_ADDR, &server_socket.sin6_addr));
	LOGD("address created!\n");

	CHK(connect(client_socket, (struct sockaddr *) &server_socket, sizeof(server_socket)));
	LOGD("Connect Succeeded!\n");

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
	int len, len2;
	Message* m_tmp;
	while(!isClosed) {
		// Now Receive Package
		bzero(buffer, MAX_BUFFER + 1);

		CHK(len = recv(client_socket, buffer, 4, 0));
		int sz = *(int*) buffer - 4;
		int i = 0;
		for(i = 0; i < sz; ++i) {
			CHK(len2 = recv(client_socket, buffer+4+i, 1, 0));
		}
//		char* b_tmp = buffer;
//		sz = len;
//		m_tmp = (Message*)buffer;
//		while(m_tmp->length > sz) {
//			LOGE("sz: %d", sz);
//			b_tmp += len;
//			CHK(len = recv(client_socket, b_tmp, m_tmp->length-sz, 0));
//			sz += len;
//		}
		LOGE("Receive %d Bytes From Server!\n", len+sz);

		// Now Parse Package
		bzero(&msg, sizeof(Message));
		memcpy(&msg, buffer, sizeof(Message));
		if(!hasIP && msg.type == MSGTYPE_IP_REC) {
			LOGD("Type: IP_REC\nContents: %s\n", msg.data);
			char b[1024] = "";
			bzero(b, sizeof(b));
			sprintf(b, "%s%d \0", msg.data, client_socket);
			len = strlen(b) + 1;
			int size = write(fifo_handle, b, len);
			if(len != size) {
				fprintf( stderr, "(line %d): ERROR - %s.\n", __LINE__, strerror( errno ) );
				exit(1);
			}

			sleep(1);

			// Now Wait for File Handle
			memset(buffer, 0, MAX_BUFFER + 1);
			len = read(fifo_handle, buffer, MAX_BUFFER);
			if(len != sizeof(int)) {
				LOGD("File Handle Read Error! Read %s (len:%d)\n", buffer, len);
				exit(1);
			}
			vpn_handle = *(int*)buffer;
			vpn_handle = ntohl(vpn_handle);
			LOGD("Get VPN Handle %d Succeeded!\n", vpn_handle);
			// Create a new thread for VPN service
			pthread_t vpn_thread;
			pthread_create(&vpn_thread, NULL, vpnService, NULL);
			hasIP = true;
		}
		else if(msg.type == MSGTYPE_DATA_RECV) {
			LOGF("Type: DATA_REC (length: %d)\nContents: %s\n", msg.length, msg.data);
			len = write(vpn_handle, msg.data, msg.length-5);
			if (len != msg.length-5) {
				LOGD("Error!\n");
			}

			// Do Stats
			pthread_mutex_lock(&traffic_mutex_in);
			in_times ++;
			in_length += len;
			pthread_mutex_unlock(&traffic_mutex_in);
		}
		else if(msg.type == MSGTYPE_HEARTBEAT) {
			LOGE("Type: HEARTBEAT\nContents: %s\n", msg.data);
			heartbeat_recv_time = time((time_t*)NULL);
		}
		else {
			LOGD("Unknown Receive Type %d!\n", msg.type);
			LOGD("Contents: %s\n", msg.data);
		}
	}
	return EXIT_SUCCESS;
}


JNIEXPORT jstring JNICALL Java_com_example_maye_IVI_MainActivity_StringFromJNI(JNIEnv *env, jobject this)
{
    return (*env)->NewStringUTF(env, "Hello World From JNI!");
}

JNIEXPORT void JNICALL Java_com_example_maye_IVI_MainActivity_IVI(JNIEnv *env, jobject this)
{
	LOGD("IVI");
    main();
}