/*
 * cgi_handler.h
 *
 *      Author: Fahad Islam
 */

#ifndef CGI_HANDLER_H_
#define CGI_HANDLER_H_

#include "lisod.h"
#include "handler.h"

// auxiliary values to help indicate the pipe fd being read
#define READ_END 0
#define WRITE_END 1

typedef struct {
	pid_t child_pid;		// pid of the execed child process
	int client_sock;		// the client socket that expects the response
//    char cgi_buf[MAX_LEN];
//    unsigned int cgi_buf_size;
    int pipe_parent2child[2]; 	// child_stdin pipe
    int pipe_child2parent[2];	// child_stdout pipe
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
     */
} cgi_client;


int handle_cgi_request(client *c, char *uri);

/* this function sets up the common environmental variables for the cgiscript
 */
int set_env_vars(client *c, char* uri);

/* sets the HTTP-Specific environmental-Variables for the cgi script by
 * parsing the request sent by the client
 *
 * return value:
 * -1 if error
 * 0 if no content-length found
 * value of content-length if its found
 */
int set_http_env_vars(client *c); //


// returns -1 if fails, success returns 0
int set_env_vars_from_uri(char *uri);




#endif /* CGI_HANDLER_H_ */
