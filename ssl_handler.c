/*
 * ssl_handler.c
 *
 *      Author: Fahad Islam
 */

#include "ssl_handler.h"

sslobj* ssl_client_list = NULL;


int close_socket(int sock);


void init_ssl(char* keyfile, char* certfile)
{
	SSL_load_error_strings();
	SSL_library_init();

	// LOG INTO FILE ABOUT WHICH CERT AND KEY WERE LOADED
	memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
	sprintf(debug_output,"loading these files %s and %s",keyfile, certfile);
	log_into_file(debug_output);

	/* we want to use TLSv1 only */
	if ((ssl_context = SSL_CTX_new(TLSv1_server_method())) == NULL)
	{
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"Error creating SSL context for TLSv1 method");
		log_into_file(debug_output);
		exit(EXIT_FAILURE);
	}

	/* register private key */
	if (SSL_CTX_use_PrivateKey_file(ssl_context, "./server.key",
			SSL_FILETYPE_PEM) == 0)
	{
		SSL_CTX_free(ssl_context);
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"Error associating private key for server");
		log_into_file(debug_output);
		exit(EXIT_FAILURE);
	}

	/* register public key (certificate) */
	if (SSL_CTX_use_certificate_file(ssl_context, "./server.crt",
			SSL_FILETYPE_PEM) == 0)
	{
		SSL_CTX_free(ssl_context);
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"Error associating certificate for server");
		log_into_file(debug_output);
		exit(EXIT_FAILURE);
	}
}


int
init_ssl_listen_socket(int https_port)
{
	int ssl_sock;
	struct sockaddr_in addr;
	int on = 1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(https_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if ((ssl_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		SSL_CTX_free(ssl_context);
		log_into_file("Failed creating ssl listen socket");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(ssl_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		log_into_file("could not setsockopt SO_REUSEADDR for HTTPS");
		exit(EXIT_FAILURE);
	}

	if (bind(ssl_sock, (struct sockaddr *) &addr, sizeof(addr)))
	{
		close_socket(ssl_sock);
		SSL_CTX_free(ssl_context);
		log_into_file("Failed binding ssl socket");
		exit(EXIT_FAILURE);
	}

	if (listen(ssl_sock, 5))
	{
		close_socket(ssl_sock);
		SSL_CTX_free(ssl_context);
		log_into_file("Error listening on ssl socket");
		exit(EXIT_FAILURE);
	}

	// log this into log file
	memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
	sprintf(debug_output,"SSL socket set up on port %d", https_port);
	log_into_file(debug_output);
	return ssl_sock;
}


void accept_ssl_client(int listen_sock, client_pool *p)
{
	int client_ssl_sock,ret_val;
	struct sockaddr_in cliaddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	client *c;
	long err_code, ssl_err_val;
	char buffer[MAXLINE];

	client_ssl_sock = accept(listen_sock, (struct sockaddr *)&cliaddr, &addrlen);
	if (client_ssl_sock == -1)
	{
		log_into_file("client accept failed from the https port");
		return;
	}
	if (fcntl(client_ssl_sock, F_SETFL, O_NONBLOCK) == -1)
	{
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"could not set ssl client socket to non-blocking "
				"for client %s",
				inet_ntoa(cliaddr.sin_addr));
		log_into_file(debug_output);
		return;
	}

	// we have got the client socket so now we wrap the connection with SSL



	sslobj* cli_ssl;
	if ( (cli_ssl = (sslobj*)malloc(sizeof(sslobj))) == NULL)
	{
		log_into_file("mallocing ssl object for new client failed");
		return;
	}
	cli_ssl->client_ssl = NULL;
	cli_ssl->sock = client_ssl_sock;
	cli_ssl->next = NULL;

	if ((cli_ssl->client_ssl = SSL_new(ssl_context)) == NULL)
	{
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"Could not create ssl object for client %s",
				inet_ntoa(cliaddr.sin_addr));
		log_into_file(debug_output);
		return;
	}

	// Connect the SSL struct to our connection
	if (SSL_set_fd(cli_ssl->client_ssl, client_ssl_sock) == 0)
	{
		SSL_free(cli_ssl->client_ssl);
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"Could not set fd for new ssl client %s.",
				inet_ntoa(cliaddr.sin_addr));
		log_into_file(debug_output);
		return;
	}

	ret_val = 0;
	while ( ret_val <= 0)
	{
		ret_val = SSL_accept(cli_ssl->client_ssl);

		ssl_err_val = SSL_get_error(cli_ssl->client_ssl, ret_val);
		switch (ssl_err_val)
		{
		case SSL_ERROR_NONE:
			//printf("ssl error none\n");
			break;
		case SSL_ERROR_SSL:
			log_into_file("ssl error ssl from ssl_accept");
			err_code = ERR_get_error();
			memset(buffer, 0, MAXLINE);
			ERR_error_string(err_code, buffer);
			memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
			sprintf(debug_output, "the error is : %s",buffer);
			log_into_file(debug_output);
			return;
		case SSL_ERROR_SYSCALL:
			log_into_file("SSL error syscall from ssl_accept");
			return;
			break;
		case SSL_ERROR_WANT_ACCEPT:
			log_into_file("SSL error want accept from ssl_accept");
			break;
		case SSL_ERROR_WANT_READ:
			//log_into_file("SSL error want read from ssl_accept");
			break;
		case SSL_ERROR_WANT_WRITE:
			//log_into_file("SSL error want write from ssl_accept");
			break;

		case SSL_ERROR_ZERO_RETURN:
			log_into_file("SSL error zero return from ssl_accept");
			break;
		default:
			log_into_file("ssl error from ssl_accept");
			err_code = ERR_get_error();
			memset(buffer, 0, MAXLINE);
			ERR_error_string(err_code, buffer);
			memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
			sprintf(debug_output, "the error is : %s",buffer);
			log_into_file(debug_output);

			memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
			sprintf(debug_output,"return vals in ssl_accept "
					"ret_frm_read:%d err:%ld ssl_get_err:%ld",
					ret_val, err_code, ssl_err_val);
			log_into_file(debug_output);
			return;
			break;
		}
	}

	// add this new ssl object to my list of current ssl clients
	LL_APPEND(ssl_client_list, cli_ssl);

	c = new_client(p, client_ssl_sock);
	c->cliaddr = cliaddr;
	c->ssl_connection = 1;
	set_hostname(c);

	memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
	sprintf(debug_output, "SSL accept turned out ok for client %d", client_ssl_sock);
	log_into_file(debug_output);
}


int read_from_ssl_client(client *c, char* inbuf, int numbytes)
{
	//printf("START SSL READ FROM SOCKET %d\n",c->sock);
	char buffer[MAX_LEN];
	memset(buffer, 0, MAX_LEN);
	int readbytes = 0, final_readbytes = 0;
	int readbyte_limit = numbytes;
	int read_blocked = 0;
	long err_val,ssl_get_error_code;
	char err_string[MAXLINE];
	char* http_end;
	sslobj* temp_ssl;
	LL_SEARCH_SCALAR(ssl_client_list,temp_ssl,sock,c->sock);

	do{
		do {
			read_blocked=0;
			readbytes=SSL_read(temp_ssl->client_ssl,buffer,readbyte_limit);
			ssl_get_error_code = SSL_get_error(temp_ssl->client_ssl,readbytes);
			switch(ssl_get_error_code)
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
			case SSL_ERROR_WANT_WRITE:
				break;
			case SSL_ERROR_SYSCALL:
				err_val = ERR_get_error();
				memset(err_string, 0, MAXLINE);
				ERR_error_string(err_val, err_string);

				memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
				sprintf(debug_output, "SSL error syscall from ssl_read: %s",
						err_string);
				log_into_file(debug_output);

				memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
				sprintf(debug_output, "return vals in ssl_read "
						"errno:%d ret_frm_read:%d err_val:%ld",
						errno, readbytes,err_val);
				log_into_file(debug_output);
				return 0;
				break;
			default:
				err_val = ERR_get_error();
				memset(err_string, 0, MAXLINE);
				ERR_error_string(err_val, err_string);

				memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
				sprintf(debug_output, "SSL read error : %s",err_string);
				log_into_file(debug_output);

				memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
				sprintf(debug_output,"return vals in ssl_read "
						"ret_frm_read:%d err:%ld ssl_get_err:%ld",
						readbytes, err_val, ssl_get_error_code);
				log_into_file(debug_output);
				return 0;
			}
			// SSL_pending() does not work correctly when a silent handshake
			// occurs, so we also put a check for read_blocked in order to
			// not stall the loop around ssl_read
		} while (!read_blocked && SSL_pending(temp_ssl->client_ssl));

		// we dont know if we received the complete http request from this
		// SSL record, so we read next record until we have the complete http
		// request or reach the number limit of bytes to read
		//printf("stats %d\n",readbytes);
		memcpy(inbuf+final_readbytes,buffer,readbytes);
		final_readbytes += readbytes;
		readbyte_limit -= readbytes;
		http_end = strstr(inbuf, "\r\n\r\n");
	} while ((http_end == NULL) && (final_readbytes < numbytes));
	//	printf("got out of do loop when ssl read finished for client %d\n",
	//			temp_ssl->sock);
	//	printf("stats num %d read %d\n", numbytes, readbytes);

	return final_readbytes;
}


void write_to_ssl_client(client *c, char* buffer, int numbytes)
{
	int ret_val = 0;
	long err_get_error_code, ssl_err_val;
	char write_error_string[MAXLINE];
	sslobj* temp_ssl;
	LL_SEARCH_SCALAR(ssl_client_list,temp_ssl,sock,c->sock);

	while (ret_val <= 0)
	{
		ret_val = SSL_write(temp_ssl->client_ssl, buffer, numbytes);
		ssl_err_val = SSL_get_error(temp_ssl->client_ssl, ret_val);
		switch (ssl_err_val)
		{
		case SSL_ERROR_NONE: break;
		case SSL_ERROR_WANT_READ: break;
		case SSL_ERROR_WANT_WRITE: break;
		default:
			err_get_error_code = ERR_get_error();
			memset(write_error_string, 0, MAXLINE);
			ERR_error_string(err_get_error_code, write_error_string);

			// NEED TO LOG SSL WRITE ERRORS INTO LOG FILE
			memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
			sprintf(debug_output, "SSL write error : %s",write_error_string);
			log_into_file(debug_output);

			memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
			sprintf(debug_output,"return vals in ssl_write "
					"ret_frm_write:%d err:%ld ssl_get_error:%ld",
					ret_val, err_get_error_code, ssl_err_val);
			log_into_file(debug_output);
			break;
		}
	}
}




void delete_client_from_ssl_list(int client_sock)
{
	sslobj* temp_ssl = NULL;
	int ret_val;
	int ssl_syscall_error = 0;
	long err_get_error_code, err_val;
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

		if (ret_val == 0)
		{
			memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
			sprintf(debug_output,"ssl shutdown return "
					"errno:%d ret_val:%d",errno, ret_val);
			log_into_file(debug_output);
			continue;
		}

		err_val = SSL_get_error(temp_ssl->client_ssl, ret_val);
		switch (err_val)
		{
		case SSL_ERROR_NONE:
			//printf("ssl error none when deleting client from ssl list\n");
			break;
		case SSL_ERROR_WANT_READ: break;
		case SSL_ERROR_WANT_WRITE: break;
		case SSL_ERROR_SYSCALL: // 5 = ssl_error_syscall
			ssl_syscall_error = 1;
			printf("SSL_shutdown got ssl_error_syscall "
					"errno:%d ret_val:%d",errno, ret_val);
			break;
			//			exit(EXIT_FAILURE);
		default:
			err_get_error_code = ERR_get_error();
			memset(buffer, 0, MAXLINE);
			ERR_error_string(err_get_error_code, buffer);
			memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
			sprintf(debug_output,"client shutdown err_str:%s",
					buffer);
			log_into_file(debug_output);

			memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
			sprintf(debug_output,"client shutdown error: ret_frm_shutdown:%d "
					"ssl_get_err:%ld err_get_error:%ld",
					ret_val, err_val, err_get_error_code);
			log_into_file(debug_output);
			return;
		}

		// syscall check and clean up all memory related to connection
		if (ssl_syscall_error == 1)
			break;
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


