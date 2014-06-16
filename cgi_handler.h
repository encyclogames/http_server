/*
 * cgi_handler.h
 *
 *      Author: Fahad Islam
 */

#ifndef CGI_HANDLER_H_
#define CGI_HANDLER_H_

#include "lisod.h"

// auxiliary values to help indicate the pipe fd being read
#define READ_END 0
#define WRITE_END 1

typedef struct {
	pid_t child_pid;
	int client_sock;
    char cgi_buf[MAX_LEN];
    unsigned int cgi_buf_size;
    int pipe_parent2child[2]; 	//child_stdin
    int pipe_child2parent[2];	//child_stdout


//    struct sockaddr_in cliaddr;
//    unsigned int inbuf_size;
//    char request_incomplete; //int
//    char close_connection; //int
//    char ssl_connection;
//    char incomplete_request_buffer[MAXLINE];
//    char hostname[MAX_HOSTNAME];
//    char inbuf[MAX_LEN];
} cgi_client;


int handle_cgi_request(client *c, char *uri);

// returns -1 if fails, success returns 0
int set_env_vars_from_uri(char *uri);

/*
 * pipe notes:
 * read from 0, write to 1
 * child closes stdin[1] and stdout[0]
 * parent closes stdin[0] and stdout[1]
 *
 * parent writes to stdin[1] to pass to child
 * child writes stdout[1] to write to parent
 *
 * parent should select on stdout[0] and shld close stdin[1]
 * when finished writing to child or u have fd leak
 *
 *
 *
 *
 */


#endif /* CGI_HANDLER_H_ */
