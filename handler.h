/*
 * handler.h
 *
 *      Author: Fahad Islam
 */

#ifndef HANDLER_H_
#define HANDLER_H_

#include "lisod.h"

// HTTP REQUEST HANDLERS
// all of them return a flag signifying whether the client just processed
// needs to be disconnected from the server or not
// 1 - close client connection and free the socket
// 0 - client is keeping a persistent http, open connection

int handle_GET(client *c, char *uri, char* www_folder);
int handle_HEAD(client *c, char *uri, char* www_folder);
int handle_POST(client *c, char *uri, char* www_folder);

void create_http_response_header(char *header, FILE *fp, char* filepath);
void get_mimetype(char *filepath, char *mimetype);
void reset_client_buffers(client *c);
void http_error(client *c, char *cause, char *errnum, char *shortmsg,
		char *longmsg, char close_connection, char send_body);

#endif /* HANDLER_H_ */
