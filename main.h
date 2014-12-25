/*
 * main.h
 *
 *      Copyright (c) <2013> <Fahad Islam>
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/ip.h>


#include "utlist.h"


#define MAX_FILENAME_LEN 128
#define MAX_HEADER_LEN 8192
#define MAX_LEN 8192
#define MAX_CLIENTS 1024
#define MAX_HOSTNAME 512
#define MAXLINE 512
#define MAX_DEBUG_MSG_LEN 256

// some auxiliary flags just to improve understanding of argument/return
// intents primarily for the http_error function in handler.c
#define CLOSE_CONN 1
#define DONT_CLOSE_CONN 0
#define SEND_HTTP_BODY 1
#define DONT_SEND_HTTP_BODY 0

typedef struct {
	int http_port;
	char log_file[MAX_FILENAME_LEN];
	char www_folder[MAX_FILENAME_LEN];
	char cgi_script[MAX_FILENAME_LEN];
	char cgi_folder[MAX_FILENAME_LEN];
} cmd_line_args;

// global scope variables
cmd_line_args cla; // the command line arguments
char debug_output[MAX_DEBUG_MSG_LEN]; // debug message to log into log file

/*
 * the client struct that stores the details of the client
 *
 * most variables of the single char datatype are used as boolean flag
 * variables that will only switch between values 0(false) and 1(true)
 *
 * "inbuf" is the internal buffer that stores the http request of the client
 *
 * "close_connection" notes whether the client has sent a "Connection:Close"
 * in its http header or indicating that my server should not remove it
 * from its persistent connection set of file descriptors
 *
 * "request_incomplete" is a variable that saves the state of a client when the
 * server receives a incomplete request from it. When a request is incomplete,
 * the server does not receive the whole request in a single read from the
 * client socket, and thus i need a way to parse through the whole request
 * before i start working on the request
 *
 * "request_incomplete" can have 5 values
 * 0 - the request is complete and the server can start serving it
 * 1 - request is incomplete and its a GET method
 * 2 - request is incomplete and its a HEAD method
 * 3 - request is incomplete and its a POST method
 * 4 - request is incomplete and its a CGI request with any method (deprecated)
 *
 */
typedef struct {
	int sock;
	struct sockaddr_in cliaddr;
	unsigned int inbuf_size;
	char close_connection;
	char request_incomplete;
	char timeout_count;
	char incomplete_request_buffer[MAXLINE];
	char hostname[MAX_HOSTNAME];
	char inbuf[MAX_LEN];
} client;

void log_into_file(char *message);
void set_hostname(client *c);

#endif /* MAIN_H_ */
