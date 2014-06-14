/*
 * ssl_handler.h
 *
 *      Author: Fahad Islam
 */

#ifndef SSL_HANDLER_H_
#define SSL_HANDLER_H_

#include "lisod.h"
#include "client_pool.h"


/*
 * I am managing my list of clients connected through SSL in the form of a
 * linked list with each node having each clients unique SSL object
*/
typedef struct sslobj_s {
	int sock;
	SSL *client_ssl;
	struct sslobj_s *next; //, *prev;
} sslobj;

void init_ssl(char* keyfile, char* certfile);
int init_ssl_listen_socket(int https_port);
//void accept_ssl_client(int sock, sslobj *s, client_pool *p);
//void read_from_ssl_socket(client *c, sslobj *ssl_client_list,
//		client_pool *p, char* buffer);

void accept_ssl_client(int sock, client_pool *p);
int read_from_ssl_client(client *c, client_pool *p, char* buffer);
void write_to_ssl_client(client *c, char* buffer, int numbytes);

void delete_client_from_ssl_list(int client_sock);


SSL_CTX *ssl_context;

#endif /* SSL_HANDLER_H_ */
