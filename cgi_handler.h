/*
 * cgi_handler.h
 *
 *      Copyright (c) <2013> <Fahad Islam>
 */

#ifndef CGI_HANDLER_H_
#define CGI_HANDLER_H_

#include "main.h"
#include "handler.h"
#include "client_pool.h"

// auxiliary values to help indicate the pipe end being read
#define READ_END 0
#define WRITE_END 1

/* struct containing details about the client issuing a cgi request and the
 * forked child that runs the cgi script, the client details are accessed using
 * the client socket from the client pool
 */
typedef struct cgi_client_s{
	pid_t child_pid;		// pid of the execed child process
	int client_sock;		// the client socket that expects the response
	int pipe_parent2child[2]; 	// child_stdin pipe
	int pipe_child2parent[2];	// child_stdout pipe
	struct cgi_client_s *next;
} cgi_client;

cgi_client* cgi_client_list;

/* the main function that handles the cgi request sent by the client. This
 * function calls the other auxiliary cgi functions to set up the server to
 * create/run the cgi script and respond to the client
 * responds 1 if the client connection needs to be closed, responds 0 otherwise
 */
int handle_cgi_request(client *c, char *uri);

/* sets up some basic CGI environmental variables for the cgi script returns -1
 * if there is any error, returns 0 otherwise
 */
int set_env_vars(client *c, char* uri);

/* sets the HTTP-Specific environmental-Variables for the cgi script by parsing
 * the request sent by the client
 *
 * return value:
 * -1 if error
 * 0 if no content-length found
 * value of content-length if its found
 */
int set_http_env_vars(client *c); //


/* sets the env vars that relate to the uri
 * it will also try to see if the uri points to a cgi script that exists
 * on the server or not,
 * returns -1 if fails, success returns 0
 */
int set_env_vars_from_uri(char *uri);

// Malloc a cgi_client struct to be used for a client with a cgi request
cgi_client * new_cgi_client(int sock);

/* handles the forking and execution of the cgi script and also the passing of
 * the message body to it through the pipe. If the cgi script fails to run for
 * any reason, the function will respond with an http error and return -1.
 * success returns 0
 */
int cgi_child_process_creator(client *c, char *message_body,
		int content_length, cgi_client *cgi_client_struct);

/* handles the reading of response from a child process runnin a cgi script
 * and piping it back to the client that requested it. The function also
 * handles error checking for the child, and cleans up the child process(wait)
 * after it has finished sending its response and terminated
 */
void transfer_response_from_cgi_to_client(cgi_client *temp_cgi, client_pool *p);

#endif /* CGI_HANDLER_H_ */
