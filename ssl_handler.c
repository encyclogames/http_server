/*
 * ssl_handler.c
 *
 *      Author: Fahad Islam
 */

#include "ssl_handler.h"

#define BUF_SIZE 4096

sslobj* ssl_client_list = NULL;


int close_socket(int sock);


void init_ssl(char* keyfile, char* certfile)
{
	SSL_load_error_strings();
	SSL_library_init();

	// LOG INTO FILE ABOUT WHICH CERT AND KEY WERE LOADED
	//printf("loading these files %s and %s\n",keyfile, certfile);

	/* we want to use TLSv1 only */
	if ((ssl_context = SSL_CTX_new(TLSv1_server_method())) == NULL)
	{
		fprintf(stderr, "Error creating SSL context for TLSv1 method\n");
		exit(EXIT_FAILURE);
	}

	/* register private key */
	if (SSL_CTX_use_PrivateKey_file(ssl_context, "./server.key",
			SSL_FILETYPE_PEM) == 0)
	{
		SSL_CTX_free(ssl_context);
		fprintf(stderr, "Error associating private key for server.\n");
		exit(EXIT_FAILURE);
	}

	/* register public key (certificate) */
	if (SSL_CTX_use_certificate_file(ssl_context, "./server.crt",
			SSL_FILETYPE_PEM) == 0)
	{
		SSL_CTX_free(ssl_context);
		fprintf(stderr, "Error associating certificate for server.\n");
		exit(EXIT_FAILURE);
	}
}


int
init_ssl_listen_socket(int https_port)
{
	int ssl_sock;
	struct sockaddr_in addr;
	int on = 1;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(https_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if ((ssl_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		SSL_CTX_free(ssl_context);
		fprintf(stderr, "Failed creating socket.\n");
		return EXIT_FAILURE;
	}

	if (setsockopt(ssl_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		perror("could not setsockopt SO_REUSEADDR for HTTPS");
		exit(-1);
	}

	if (bind(ssl_sock, (struct sockaddr *) &addr, sizeof(addr)))
	{
		close_socket(ssl_sock);
		SSL_CTX_free(ssl_context);
		fprintf(stderr, "Failed binding socket.\n");
		return EXIT_FAILURE;
	}

	if (listen(ssl_sock, 5))
	{
		close_socket(ssl_sock);
		SSL_CTX_free(ssl_context);
		fprintf(stderr, "Error listening on socket.\n");
		return EXIT_FAILURE;
	}

	// log this into log file
	//printf("SSL socket set up on port %d\n", https_port);
	return ssl_sock;
}


void accept_ssl_client(int listen_sock, client_pool *p)
{
	int client_ssl_sock,ret_val,err_val;
	struct sockaddr_in cliaddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	client *c;

	client_ssl_sock = accept(listen_sock, (struct sockaddr *)&cliaddr, &addrlen);
	if (client_ssl_sock == -1) {
		perror("client accept failed from the https port");
		//		clienterror(c->sock, "GET", "501", "Not Implemented", "This method unimplemented");
		return;
	}
	if (fcntl(client_ssl_sock, F_SETFL, O_NONBLOCK) == -1) {
		printf("could not set ssl client socket to non-blocking for client %s",
				inet_ntoa(cliaddr.sin_addr));
		return;
	}

	// we have got the client socket so now we wrap the connection with SSL //

	sslobj* cli_ssl;
	if ( (cli_ssl = (sslobj*)malloc(sizeof(sslobj))) == NULL)
		printf("mallocing ssl object for new client failed");
	cli_ssl->client_ssl = NULL;
	cli_ssl->sock = client_ssl_sock;
	cli_ssl->next = NULL;

	if ((cli_ssl->client_ssl = SSL_new(ssl_context)) == NULL)
	{
		printf("Could not create ssl object for client %s",
				inet_ntoa(cliaddr.sin_addr));
		return;
	}

	// Connect the SSL struct to our connection
	if (SSL_set_fd(cli_ssl->client_ssl, client_ssl_sock) == 0)
	{
		SSL_free(cli_ssl->client_ssl);
		printf("Could not set fd for new ssl client %s.\n",
				inet_ntoa(cliaddr.sin_addr));
		return;
	}


//	printf("TRY SSL ACCEPT.\n");
	ret_val = 0;
	while ( ret_val <= 0)
	{
		ret_val = SSL_accept(cli_ssl->client_ssl);

		err_val = SSL_get_error(cli_ssl->client_ssl, ret_val);
		switch (err_val)
		{
		case SSL_ERROR_NONE:
			//printf("ssl error none\n");
			break;
		case SSL_ERROR_SSL:
			printf("ssl error ssl\n");
			long bla = ERR_get_error();
			char buffer[MAXLINE];
			memset(buffer, 0, MAXLINE);
			ERR_error_string(bla, buffer);
			printf("the error is : %s\n",buffer);
			return;
		case SSL_ERROR_SYSCALL: printf("SSL error syscall\n"); return; break;
		case SSL_ERROR_WANT_ACCEPT: printf("SSL error want accept\n"); break;
		//case SSL_ERROR_WANT_READ: printf("SSL error want read\n"); break;
		case SSL_ERROR_ZERO_RETURN: printf("SSL error zero return\n"); break;
		}
	}

	// add this new ssl object to my list of current ssl clients
	LL_APPEND(ssl_client_list, cli_ssl);

	//	DPRINTF(DEBUG_SOCKETS|DEBUG_CLIENTS, "Got client on socket %d\n", clisock);
	c = new_client(p, client_ssl_sock);
	c->cliaddr = cliaddr;
	c->ssl_connection = 1;
	set_hostname(c);

	printf("SSL accept turned out ok for client %d\n", client_ssl_sock);
}


//int read_from_ssl_client(client *c, client_pool *p, char* inbuf)
//{
//	//printf("START SSL READ FROM SOCKET %d\n",c->sock);
//	char buffer[MAX_LEN];
//	memset(buffer, 0, MAX_LEN);
//	int readbytes = 0, final_readbytes = 0;
//	int readbyte_limit = MAX_LEN;
//	int read_blocked = 0,offset = 0;
//	long err_val;
//	char err_string[MAXLINE];
//	char* http_end;
//	sslobj* temp_ssl;
//	LL_SEARCH_SCALAR(ssl_client_list,temp_ssl,sock,c->sock);
//
//	////////////////////////////////////////////
//	// safety code for python ssl client sanity check
//	// TO DO: fix the fd closing issue in order to not use this
//	//	if ((temp_ssl->next))
//	//		temp_ssl=(temp_ssl->next);
//
//	/////////////////   BASIC SSL ECHO FOR PYTHON TEST
//	//	int readret = 0;
//	//	char buf[BUF_SIZE];
//	//	while((readret = SSL_read(temp_ssl->client_ssl, buf, BUF_SIZE)) > 0)
//	//	{
//	//		SSL_write(temp_ssl->client_ssl, buf, readret);
//	//		memset(buf, 0, BUF_SIZE);
//	//	}
//	//return 0;
//
//	/////////////////////////////////////////////
//
//
//	////////////////// MUCH IMPROVED VERSION
//	//////////////////  NOTE: DOES NOT HAVE ECHO SUPPORT FOR PYTHON TEST
//	do{
//		do {
//			read_blocked=0;
//			readbytes=SSL_read(temp_ssl->client_ssl,buffer,readbyte_limit);
//			switch(SSL_get_error(temp_ssl->client_ssl,readbytes))
//			{
//			case SSL_ERROR_NONE:
//				//printf("got this from ssl: %s\n", buffer);
//				break;
//			case SSL_ERROR_ZERO_RETURN:
//				// End of data or client has no request
//				return 0;
//				break;
//			case SSL_ERROR_WANT_READ:
//				read_blocked=1;
//				break;
//			default:
//				printf("SSL read problem\n");
//				err_val = ERR_get_error();
//				memset(err_string, 0, MAXLINE);
//				ERR_error_string(err_val, err_string);
//				printf("the error is : %s\n",err_string);
//				return 0;
//			}
//			// We need a check for read_blocked here because SSL_pending()
//			// doesn’t work properly during a silent handshake. This check
//			// prevents a busy-wait loop around SSL_read()
//		} while (SSL_pending(temp_ssl->client_ssl) && !read_blocked);
//
//		// we dont know if we received the complete http request from this
//		// SSL record, so we read next record until we have the complete http
//		// request or reach a cap of 8 Kilobytes read
//		//printf("stats %d\n",readbytes);
//		memcpy(inbuf+final_readbytes,buffer,readbytes);
//		final_readbytes += readbytes;
//		readbyte_limit -= readbytes;
//		http_end = strstr(inbuf, "\r\n\r\n");
//	} while ((http_end == NULL) && (final_readbytes < MAX_LEN));
//	//	printf("got out of do loop when ssl read finished for client %d\n",
//	//			temp_ssl->sock);
//	//	printf("stats %d\n",
//	//			readbytes);
//
//	return final_readbytes;
//}

int read_from_ssl_client(client *c, client_pool *p, char* inbuf, int numbytes)
{
	//printf("START SSL READ FROM SOCKET %d\n",c->sock);
	char buffer[MAX_LEN];
	memset(buffer, 0, MAX_LEN);
	int readbytes = 0, final_readbytes = 0;
	int readbyte_limit = numbytes; // MAX_LEN
	int read_blocked = 0,offset = 0;
	long err_val;
	char err_string[MAXLINE];
	char* http_end;
	sslobj* temp_ssl;
	LL_SEARCH_SCALAR(ssl_client_list,temp_ssl,sock,c->sock);

	////////////////////////////////////////////
	// safety code for python ssl client sanity check
	// TO DO: fix the fd closing issue in order to not use this
	//	if ((temp_ssl->next))
	//		temp_ssl=(temp_ssl->next);

	/////////////////   BASIC SSL ECHO FOR PYTHON TEST
	//	int readret = 0;
	//	char buf[BUF_SIZE];
	//	while((readret = SSL_read(temp_ssl->client_ssl, buf, BUF_SIZE)) > 0)
	//	{
	//		SSL_write(temp_ssl->client_ssl, buf, readret);
	//		memset(buf, 0, BUF_SIZE);
	//	}
	//return 0;

	/////////////////////////////////////////////


	////////////////// MUCH IMPROVED VERSION
	//////////////////  NOTE: DOES NOT HAVE ECHO SUPPORT FOR PYTHON TEST
	do{
		do {
			read_blocked=0;
			readbytes=SSL_read(temp_ssl->client_ssl,buffer,readbyte_limit);
			switch(SSL_get_error(temp_ssl->client_ssl,readbytes))
			{
			case SSL_ERROR_NONE:
				//printf("got this from ssl: %s\n", buffer);
				break;
			case SSL_ERROR_ZERO_RETURN:
				// End of data or client has no request
				return 0;
				break;
			case SSL_ERROR_WANT_READ:
				read_blocked=1;
				break;
			default:
				printf("SSL read problem\n");
				err_val = ERR_get_error();
				memset(err_string, 0, MAXLINE);
				ERR_error_string(err_val, err_string);
				printf("the error is : %s\n",err_string);
				return 0;
			}
			// We need a check for read_blocked here because SSL_pending()
			// doesn’t work properly during a silent handshake. This check
			// prevents a busy-wait loop around SSL_read()
		} while (SSL_pending(temp_ssl->client_ssl) && !read_blocked);

		// we dont know if we received the complete http request from this
		// SSL record, so we read next record until we have the complete http
		// request or reach a cap of 8 Kilobytes read
		//printf("stats %d\n",readbytes);
		memcpy(inbuf+final_readbytes,buffer,readbytes);
		final_readbytes += readbytes;
		readbyte_limit -= readbytes;
		http_end = strstr(inbuf, "\r\n\r\n");
	} while ((http_end == NULL) && (final_readbytes < numbytes)); // MAX_LEN
	//	printf("got out of do loop when ssl read finished for client %d\n",
	//			temp_ssl->sock);
	//	printf("stats %d\n",
	//			readbytes);

	return final_readbytes;
}


void write_to_ssl_client(client *c, char* buffer, int numbytes)
{
	int ret_val = 0, err_val;
	long bla;
	char write_error_string[MAXLINE];
	sslobj* temp_ssl;
	LL_SEARCH_SCALAR(ssl_client_list,temp_ssl,sock,c->sock);

	while (ret_val <= 0)
	{
		ret_val = SSL_write(temp_ssl->client_ssl, buffer, numbytes);
		err_val = SSL_get_error(temp_ssl->client_ssl, ret_val);
		switch (err_val)
		{
		case SSL_ERROR_WANT_READ: break;
		case SSL_ERROR_WANT_WRITE: break;
		default:
			bla = ERR_get_error();
			memset(write_error_string, 0, MAXLINE);
			ERR_error_string(bla, write_error_string);
			// NEED TO LOG SSL WRITE ERRORS INTO LOG FILE
			//			printf("error in ssl write: %s\n",write_buffer);
			//			return;
			break;
		}
	}
}




void delete_client_from_ssl_list(int client_sock)
{
	sslobj* temp_ssl = NULL;
	int ret_val, err_val;
	long bla;
	char buffer[MAXLINE];


	LL_SEARCH_SCALAR(ssl_client_list,temp_ssl,sock,client_sock);

	// if this client has no ssl connection, then do not proceed
	// with the function
	if (temp_ssl == NULL)
		return;


	LL_DELETE(ssl_client_list, temp_ssl);

	// free temp ssl and close its ssl object
	ret_val = 0;
	while ( ret_val <= 0)
	{
		ret_val = SSL_shutdown(temp_ssl->client_ssl);

		err_val = SSL_get_error(temp_ssl->client_ssl, ret_val);
		switch (err_val)
		{
		case SSL_ERROR_NONE:
			//printf("ssl error none when deleting client from ssl list\n");
			break;
		case SSL_ERROR_WANT_READ: break;
		case SSL_ERROR_WANT_WRITE: break;
		default:
			bla = ERR_get_error();
			memset(buffer, 0, MAXLINE);
			ERR_error_string(bla, buffer);
			printf("error in delete client shutdown: %s",buffer);
			return;
		}
	}
	SSL_free(temp_ssl->client_ssl);
	free (temp_ssl);
}

int close_socket(int sock)
{
	if (close(sock))
	{
		fprintf(stderr, "Failed closing socket.\n");
		return 1;
	}
	return 0;
}


