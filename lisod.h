/*
 * lisod.h
 *
 *      Author: Fahad Islam
 */

#ifndef LISOD_H_
#define LISOD_H_

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
#include <netinet/in.h>
#include <netinet/ip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


#include "utlist.h"


#define MAX_FILENAME_LEN 128
#define MAX_HEADER_LEN 8192  //test scenario for pipeline uses 128
#define MAX_LEN 8192
#define MAX_CLIENTS 1024
#define MAX_HOSTNAME 512
#define MAXLINE 512

// some auxiliary flags just to improve understanding of argument intents
// primarily for the http_error function in handler.c
#define CLOSE_CONN 1
#define DONT_CLOSE_CONN 0
#define SEND_HTTP_BODY 1
#define DONT_SEND_HTTP_BODY 0

typedef struct {
    int http_port;
    int https_port;
    char log_file[MAX_FILENAME_LEN];
    char lock_file[MAX_FILENAME_LEN];
    char www_folder[MAX_FILENAME_LEN];
    char cgi_folder[MAX_FILENAME_LEN];
    char private_key_file[MAX_FILENAME_LEN];
    char certificate_file[MAX_FILENAME_LEN];
} cmd_line_args;

cmd_line_args cla;
/*
 * the client struct that stores the details of the client
 *
 * most variables of the single char datatype are designed as boolean
 * flag variables that will only switch between values 0(false) and 1(true)
 *
 * inbuf = client internal buffer
 *
 * close_connection notes whether the client has sent a "Connection:Close"
 * in its http header or not indicating that my server should remove it
 * from its persistent connection set of file descriptors
 *
 * ssl_connection is a flag variable that indicates whether the client is using
 * an ssl connection or not when connecting to the server. This is necessary
 * for me to be able to wrap the connection with the ssl API.
 *
 * request_incomplete is a variable that saves the state of a client when the
 * server receives a pipelined request from it. When a request is pipelined,
 * the server does not receive the whole request in a single read from the
 * client socket, and thus i need a way to parse through the whole request
 * before i start working on the request
 *
 * request_incomplete can have 4 values
 * 0 - the request is complete and the server can start serving it
 * 1 - request is incomplete and its a GET method
 * 2 - request is incomplete and its a HEAD method
 * 3 - request is incomplete and its a POST method
 *
 */
typedef struct {
    int sock;
    struct sockaddr_in cliaddr;
    unsigned int inbuf_size;
    char request_incomplete; //int
    char close_connection; //int
    char ssl_connection;
    char incomplete_request_buffer[MAXLINE];
    char hostname[MAX_HOSTNAME];
    char inbuf[MAX_LEN];
} client;

void log_into_file(char *message);
void set_hostname(client *c);

#endif /* LISOD_H_ */
