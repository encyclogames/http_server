/*
 ============================================================================
 Name        : Project 1 : HTTP Liso Web Server
 Author      : Fahad Islam
 Description : Implementation of Project 1
 ============================================================================
 */

#include "lisod.h"
#include "client_pool.h"
#include "handler.h"
#include "ssl_handler.h"
#include "cgi_handler.h"

char *hostname;

// server setup auxiliary functions
void init_from_command_line(int argc, char **argv);
void web_server();
void handle_input(client *c, client_pool *p);
void server_loop(int http_sock, int https_sock, client_pool *p);

// client managing functions
void accept_client(int sock, client_pool *p);
void disconnect_client(client *c, client_pool *p);
void timeout_client(client *c, client_pool *p);

// Other utility functions
int daemonize(char* lock_file);
void clear_bad_fd(client_pool *p);

/*
 * Prints the command line format of the lisod program and exits immediately
 */
void usage() {
	fprintf(stderr, "usage: ./lisod <HTTP port> <HTTPS port> <log file> <lock file> "
			"<www folder> <CGI folder or script name> <private key file> <certificate file>\n");
	exit(EXIT_FAILURE);
}

/*
 * initialize the details of the web server using the information from the command line
 */
void init_from_command_line(int argc, char **argv)
{
	cla.http_port = atoi(argv[1]);
	cla.https_port= atoi(argv[2]);
	strcpy(cla.log_file, argv[3]);
	strcpy(cla.lock_file, argv[4]);
	strcpy(cla.www_folder, argv[5]);

	//	strcpy(cla.cgi_script, argv[6]);
	struct stat cgi_stat;
	stat(argv[6], &cgi_stat);
	if (S_ISDIR(cgi_stat.st_mode)) // we have a cgi directory
	{
		strcpy(cla.cgi_folder, argv[6]);
		memset(cla.cgi_script, 0, MAX_FILENAME_LEN);
	}
	else // we have a cgi script that handles all cgi requests
	{
		strcpy(cla.cgi_script, argv[6]);
		memset(cla.cgi_folder, 0, MAX_FILENAME_LEN);
	}
	memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
	sprintf(debug_output,"cla folder:%s cla script:%s\n",
			cla.cgi_folder, cla.cgi_script);
	log_into_file(debug_output);


	strcpy(cla.private_key_file, argv[7]);
	strcpy(cla.certificate_file, argv[8]);

	FILE *fp;
	fp = fopen(cla.log_file,"w"); // write mode
	if( fp == NULL )
		return;
	time_t rawtime;
	struct tm *tm_date;
	char time_str[256];
	time( &rawtime );
	tm_date = localtime( &rawtime );
	memset(time_str,0,256);
	strftime(time_str,256,"%a, %e %b %Y %H:%M:%S", tm_date);
	fprintf(fp, "Log file for Lisod Server started on %s\r\n", time_str);
	fclose(fp);
}


int main( int argc, char *argv[] )
{
	if (argc < 8) usage();

	// initialise global vars
	init_from_command_line(argc, argv);
	init_ssl(cla.private_key_file, cla.certificate_file);
	cgi_client_list = NULL;

	// First daemonize the server process
	daemonize(cla.lock_file);

	web_server();
	return 0;
}

int
init_socket(int http_port)
{
	int s;
	struct sockaddr_in sin;
	int on = 1;

	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(http_port);
	sin.sin_addr.s_addr = INADDR_ANY;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("could not create http socket");
		exit(-1);
	}

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		perror("could not setsockopt SO_REUSEADDR for http");
		exit(-1);
	}

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1) {
		perror("could not set http socket to non-blocking");
		exit(-1);
	}

	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		perror("could not bind http socket");
		exit(-1);
	}

	if (listen(s, 15) == -1) {
		perror("could not listen on http socket");
		exit(-1);
	}

	return s;
}

void
init_hostname() {
#define HOSTNAME_LEN 32

	if (!(hostname = (char *)malloc(HOSTNAME_LEN))) {
		perror("could not allocate");
		exit(-1);
	}
	if (gethostname(hostname, HOSTNAME_LEN) < 0) {
		perror("error getting local hostname");
		exit(-1);
	}
}

void
web_server() {
	int listen_http_socket, listen_https_socket;
	client_pool *p = client_pool_create();
	listen_http_socket = init_socket(cla.http_port);
	listen_https_socket = init_ssl_listen_socket(cla.https_port);
	init_hostname();
	server_loop(listen_http_socket, listen_https_socket, p);
}

void
server_loop(int http_sock, int https_sock,client_pool *p)
{
	fd_set readfds;
	fd_set writefds;
	struct timeval timeout;
	int i, maxfd, rc, wait_status;
	client *c;
	cgi_client *cgi_itr;
	signal(SIGPIPE, SIG_IGN);

	while (1)
	{
		timeout.tv_sec = 10; // select timeouts every 10 seconds
		timeout.tv_usec = 0;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		FD_SET(http_sock, &readfds);
		FD_SET(https_sock, &readfds);
		maxfd = (http_sock > https_sock) ? http_sock : https_sock;

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			client *c;
			c = p->clients[i];
			if (c == NULL)
				continue;

			FD_SET(c->sock, &readfds);
			if (c->sock > maxfd) maxfd = c->sock;
		}

		// SET FDS FOR EXECED CGI CHILDREN
		i = 0;
		LL_FOREACH(cgi_client_list, cgi_itr)
		{
			FD_SET(cgi_itr->pipe_child2parent[READ_END], &readfds);
			if (cgi_itr->pipe_child2parent[READ_END] > maxfd)
				maxfd = cgi_itr->pipe_child2parent[READ_END];
		}

		rc = select(maxfd+1, &readfds, NULL, NULL, &timeout);

		if (rc == -1) // select error
		{
			if (errno == EBADF)
			{
				memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
				sprintf(debug_output,"Select error, "
						"bad file descriptor in sets");
				log_into_file(debug_output);

				exit(1);
			}
			else if (errno == EINTR)
			{
				perror("Select was interrupted by signal");
			}
			continue; // graceful handle rather than exit_failure
		}

		if (rc == 0) // select timeout
		{
			for (i = 0; i < MAX_CLIENTS; i++)
			{

				c = p->clients[i];
				if (c == NULL)
					continue;

				timeout_client(c, p);
			}
		}

		if (FD_ISSET(http_sock, &readfds))
		{
			accept_client(http_sock, p);
		}

		if (FD_ISSET(https_sock, &readfds))
		{
			accept_ssl_client(https_sock, p);
		}

		for (i = 0; i < MAX_CLIENTS; i++)
		{

			c = p->clients[i];
			if (c == NULL)
				continue;

			if (FD_ISSET(c->sock, &readfds))
				handle_input(c, p);
		}

		// send response from cgi child to the client
		LL_FOREACH(cgi_client_list, cgi_itr)
		{
			if (FD_ISSET(cgi_itr->pipe_child2parent[READ_END], &readfds))
			{
				transfer_response_from_cgi_to_client(cgi_itr, p);
			}

		}

		// wait on any defunct cgi children not captured by other waits
		waitpid(-1, &wait_status, WNOHANG);

	}
}

/* Nit-picky note:  the gethostbyaddr function is technically a blocking
 * function.  The grading scripts will accept this;  most async
 * DNS lookup routines are a bit painful in C. */
void
set_hostname(client *c)
{
	char addrbuf[32];
	inet_ntop(AF_INET, &c->cliaddr.sin_addr, addrbuf, sizeof(addrbuf));

	struct hostent *he;
	if ((he = gethostbyaddr(&c->cliaddr.sin_addr, sizeof(struct sockaddr_in),
			AF_INET)) != NULL) {
		strncpy(c->hostname, he->h_name, MAX_HOSTNAME);
	} else {
		strncpy(c->hostname, addrbuf, MAX_HOSTNAME);
	}
}

void accept_client(int sock, client_pool *p)
{
	int clisock;
	struct sockaddr_in cliaddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	client *c;

	clisock = accept(sock, (struct sockaddr *)&cliaddr, &addrlen);
	if (clisock == -1) {
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"client accept failed from the http port");
		log_into_file(debug_output);
		return;
	}
	if (fcntl(clisock, F_SETFL, O_NONBLOCK) == -1) {
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"could not set client socket to "
				"non-blocking for client %s",
				inet_ntoa(cliaddr.sin_addr));
		log_into_file(debug_output);

		return;
	}

	c = new_client(p, clisock);
	c->cliaddr = cliaddr;
	c->ssl_connection = 0;
	set_hostname(c);
}

void disconnect_client(client *c, client_pool *p)
{
	//only disconnect if client sent a connection close request in http
	if (c->close_connection == DONT_CLOSE_CONN)
		return;
	if (c->ssl_connection == 1)
		delete_client_from_ssl_list(c->sock);
	close(c->sock);
	delete_client(p, c->sock);
}

void handle_input(client *c, client_pool *p)
{
	const char* req_ver = "HTTP/1.1";
	const char* method_GET = "GET";
	const char* method_HEAD = "HEAD";
	const char* method_POST = "POST";

	char inbuf[MAX_HEADER_LEN+1];
	int nread = 0, num_matches;
	int close_connection = 0;
	char* request_line;
	char* inbufptr = inbuf;
	char method[10], uri[MAXLINE], version[12] ;

	memset(inbuf, 0, MAX_HEADER_LEN+1);
	memset(method, 0, 10);
	memset(uri, 0, MAXLINE);
	memset(version, 0, 12);

	// put in call for ssl read function to transfer data to inbuf
	if (c->ssl_connection == 1)
	{
		nread = read_from_ssl_client(c, inbufptr, sizeof(inbuf)-1);
		//printf("out of ssl read in handle input: %sEND length = %d\n",
		// inbufptr, nread);
	}
	else
	{
		nread = read(c->sock, inbuf, sizeof(inbuf)-1);
		inbufptr = inbuf;
		//printf("out of basic read in handle input: %sEND length = %d\n",
		//inbufptr, nread );
	}

	if (nread == 0)
	{
		// log which client is being disconnected
		char remote_addr[20] = "";
		char *client_ip = inet_ntoa(c->cliaddr.sin_addr);
		strcat(remote_addr, client_ip);
		//printf("nread 0 disconnecting client ");
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"read 0 bytes, disconnecting client %s\n",
				client_ip);
		log_into_file(debug_output);

		c->close_connection = CLOSE_CONN;
		disconnect_client(c, p);
		return;
	}
	if (nread == -1)
	{
		http_error( c, "Error from reading client socket", "500",
				"Internal Server Error",
				"An internal server error has occurred ",
				CLOSE_CONN, SEND_HTTP_BODY);
		disconnect_client(c, p);
		return;
	}

	// received client request, so reset its timeout counter
	c->timeout_count = 0;

	// check if we receive a broken/malformed request
	if (strstr(inbuf,"\r\n") == NULL)
	{
		http_error( c, "Client has been disconnected from server", "400",
				"Bad Request",
				"broken HTTP request received", CLOSE_CONN, SEND_HTTP_BODY);
		disconnect_client(c, p);
		return;

	}
	////////////////////////////////////    ECHO CODE FOR CHECKPOINT 1
	// write( c->sock, inbuf, nread);
	// return;
	////////////////////////////////////    END ECHO CODE FOR CHECKPOINT 1

	// make a copy of the request into the buffer of the client
	// using strncat instead of strncpy because strncat handles lack of null
	// characters gracefully and so, i dont need to worry about edge cases
	//	printf("client existing inbuf : %s\n",c->inbuf);
	inbufptr = inbuf;
	strncat(c->inbuf, inbufptr, nread);
	c->inbuf_size = nread;

	// if the previous request was incomplete, then the client struct already
	// knows, so i directly jump into the function that handles the incomplete
	// request
	switch(c->request_incomplete)
	{
	case 1: close_connection = handle_GET(c, uri, cla.www_folder);
	if (close_connection == 1)
		disconnect_client(c,p);
	return;
	break;
	case 2: close_connection = handle_HEAD(c, uri, cla.www_folder);
	if (close_connection == 1)
		disconnect_client(c,p);
	return;
	break;
	case 3: close_connection = handle_POST(c, uri, cla.www_folder);
	if (close_connection == 1)
		disconnect_client(c,p);
	return;
	break;
	case 4:close_connection = handle_cgi_request(c, uri);
	if (close_connection == 1)
		disconnect_client(c,p);
	return;
	default: break;
	}

	// we come here if the request is a fresh one (not a request that is too big
	// for the read buffer and thus got pipelined )

	// now start parsing the first line of the request for correct HTTP
	// version and finding out what request method has been received
	request_line = strtok(inbuf, "\r\n");

	num_matches = sscanf(request_line, "%s %s %s", method, uri, version);

	//	memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
	//	sprintf(debug_output,"method %s uri %s version %s \n",
	//			method, uri, version);
	//	log_into_file(debug_output);


	// check to see if we actually received a proper http request line
	if (num_matches != 3)
	{
		http_error( c, "Client has been disconnected from server", "400",
				"Bad Request",
				"broken HTTP request received", CLOSE_CONN, SEND_HTTP_BODY);
		disconnect_client(c, p);
		return;
	}

	/******************************* CGI BRANCH ******************************/
	if (strstr(uri, "/cgi/"))
	{
		close_connection = handle_cgi_request(c, uri);
		if (close_connection == 1)
		{
			disconnect_client(c,p);
		}
		return;
	}



	// first check for HEAD request since it affects how our error response
	// will be. HEAD request does not expect an HTML entity detailing the error
	if (strcmp(method, method_HEAD) == 0)
	{
		// check whether proper HTTP version is supported or not
		if (strcmp(version, req_ver) != 0)
		{
			http_error(c, version, "505", "HTTP Version Not Supported",
					"This server accepts HTTP requests of the following version only",
					CLOSE_CONN, DONT_SEND_HTTP_BODY);
			disconnect_client(c, p);
			return;
		}
		else
		{
			close_connection = handle_HEAD(c, uri, cla.www_folder);
		}
	}
	// non-HEAD requests require an HTML entity if error occurs
	// check whether proper HTTP version is supported or not while sending HTML entity
	else if (strcmp(version, req_ver) != 0)
	{
		http_error(c, version, "505", "HTTP Version Not Supported",
				"This server accepts HTTP requests of the following version only",
				CLOSE_CONN, SEND_HTTP_BODY);
		disconnect_client(c, p);
		return;
	}
	// check if a supported method is asked for in the HTTP request
	else if (strcmp(method, method_GET) == 0)
	{
		close_connection = handle_GET(c, uri, cla.www_folder);
	}
	else if (strcmp(method, method_POST) == 0)
	{
		close_connection = handle_POST(c, uri, cla.www_folder);
	}
	else
	{
		http_error(c, method, "501", "Not Implemented",
				"This server only handles GET,HEAD and POST requests. "
				"This method is unimplemented", DONT_CLOSE_CONN, SEND_HTTP_BODY);
		return;
	}
	if (close_connection == CLOSE_CONN)
	{
		disconnect_client(c,p);
	}
}

void timeout_client(client *c, client_pool *p)
{
	c->timeout_count ++;
	if (c->timeout_count >=5)
	{
		c->close_connection = CLOSE_CONN;
		disconnect_client(c, p);
	}
}

void log_into_file(char *message)
{
	FILE *fp;
	fp = fopen(cla.log_file,"a"); // append mode
	if( fp == NULL )
		return;

	// time of log
	time_t rawtime;
	struct tm *tm_date;
	char time_str[256];
	time( &rawtime );
	tm_date = localtime( &rawtime );
	memset(time_str,0,256);
	strftime(time_str,256,"[%e-%b-%Y %T %Z]:", tm_date);

	// log the message
	fprintf(fp, "%s %s\n", time_str, message);

	fclose(fp);
}


/***** Utility Functions for daemonizing a process below *****/

/**
 * internal signal handler
 */
void signal_handler(int sig)
{
	switch(sig)
	{
	case SIGHUP:
		/* rehash the server */
		break;
	case SIGKILL:
		printf("got sigkill\n");
		exit(0);
		break;
	case SIGTERM:
		/* finalize and shutdown the server */
		// TODO: liso_shutdown(NULL, EXIT_SUCCESS);
		printf("got sigterm\n");
		exit(0);
		break;
	default:
		break;
		/* unhandled signal */
	}
}

/**
 * internal function daemonizing the process
 */
int daemonize(char* lock_file)
{
	/* drop to having init() as parent */
	int i, lfp, pid = fork();
	char str[256] = {0};
	if (pid < 0) exit(EXIT_FAILURE);
	if (pid > 0) exit(EXIT_SUCCESS);

	setsid();

	for (i = getdtablesize(); i>=0; i--)
		close(i);

	i = open("/dev/null", O_RDWR);
	dup(i); /* stdout */
	dup(i); /* stderr */
	umask(027);

	lfp = open(lock_file, O_RDWR|O_CREAT|O_EXCL, 0640);

	if (lfp < 0)
	{
		lfp = open(lock_file, O_RDWR|O_CREAT, 0640);

		if (lfp < 0)
		{
			memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
			sprintf(debug_output,"Could not open lock file for lisod daemon");
			log_into_file(debug_output);
			exit(EXIT_FAILURE); /* can not open */
		}
	}

	if (lockf(lfp, F_TLOCK, 0) < 0)
	{
		memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
		sprintf(debug_output,"Could not lock file for lisod daemon");
		log_into_file(debug_output);
		exit(EXIT_SUCCESS); /* can not lock */
	}

	/* only first instance continues */
	sprintf(str, "%d\n", getpid());
	write(lfp, str, strlen(str)); /* record pid to lockfile */

	signal(SIGCHLD, SIG_IGN); /* child terminate signal */

	signal(SIGHUP, signal_handler); /* hangup signal */
	signal(SIGTERM, signal_handler); /* software termination signal from kill */

	// log into file --> "Successfully daemonized lisod process, pid %d."
	memset(debug_output, 0, MAX_DEBUG_MSG_LEN);
	sprintf(debug_output,"Successfully daemonized lisod process, pid %d.", pid);
	log_into_file(debug_output);

	return EXIT_SUCCESS;
}

/* Search for bad file descriptor and remove it from memory along with its
 * associated data structures
 */
void clear_bad_fd(client_pool *p)
{
	printf("starting cleanup\n");
	int i;
	client *c;
	cgi_client *cgi_itr;

	// check for bad fd in the client list
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		c = p->clients[i];
		if (c == NULL)
			continue;

		if (fcntl(c->sock, F_GETFD) == -1)
		{
			if (errno == EBADFD) // so the client socket is a bad fd
			{
				c->close_connection = CLOSE_CONN;
				disconnect_client(c, p);
			}
		}
	}

	LL_FOREACH(cgi_client_list, cgi_itr)
	{
		if (fcntl(cgi_itr->pipe_child2parent[READ_END], F_GETFD) == -1)
		{
			if (errno == EBADFD) // cgi child pipe socket is broken
			{
				printf("got broken child");
				// transfer function handles child waiting and cleanup
				transfer_response_from_cgi_to_client(cgi_itr, p);
			}
		}
	}
}
