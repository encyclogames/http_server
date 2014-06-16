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

//sslobj* ssl_client_list = NULL;

// server setup auxiliary functions
void init_from_command_line(int argc, char **argv);
void web_server();
void handle_input(client *c, client_pool *p);
void server_loop(int http_sock, int https_sock, client_pool *p);

// client managing functions
void accept_client(int sock, client_pool *p);
void disconnect_client(client *c, client_pool *p);

// Other utility functions
int daemonize(char* lock_file);

/*
 * Prints the command line format of the lisod program and exits immediately
 */
void usage() {
	fprintf(stderr, "usage: ./lisod <HTTP port> <HTTPS port> <log file> <lock file> "
			"<www folder> <CGI folder or script name> <private key file> <certificate file>\n");
	exit(-1);
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

	strcpy(cla.cgi_script, argv[6]);
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

	init_from_command_line(argc, argv);

	// First daemonize the server process
	// daemonize(cla.lock_file);
	init_ssl(cla.private_key_file, cla.certificate_file);
	//printf( "Listening on port %d for new clients \n", cla.http_port);
	//fflush(stdout);
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
	int i, maxfd, rc;
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

		// go through ssl client list and set socket in read fd
		// TO DO
		//		LL_FOREACH(ssl_client_list, ssl_itr)
		//		{
		//			FD_SET(ssl_itr->sock, &readfds);
		//			if (ssl_itr->sock > maxfd) maxfd = ssl_itr->sock;
		//		}

		rc = select(maxfd+1, &readfds, NULL, NULL, &timeout);
		if (rc == -1)
		{
			if (errno == EBADF)
				perror("Select error, bad file descriptor in sets");
			else if (errno == EINTR)
				perror("Select was interrupted by signal");
			//exit(EXIT_FAILURE); // graceful reset
			continue;
		}

		if (FD_ISSET(http_sock, &readfds))
		{
			accept_client(http_sock, p);
		}

		if (FD_ISSET(https_sock, &readfds))
		{
			accept_ssl_client(https_sock, p);
			//			printf("finished function accept ssl client\n");
			//			sslobj* temp_ssl;
			//			int count = 0;
			//			LL_FOREACH(ssl_client_list,temp_ssl)
			//			{
			//				printf("in isset %d\n", temp_ssl->sock);
			//				count++;
			//			}
			//
			//			printf("finished printing socks of ssl clients count = %d\n",count);
		}

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			client *c;
			c = p->clients[i];
			if (c == NULL)
				continue;

			if (FD_ISSET(c->sock, &readfds))
				handle_input(c, p);
		}
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
	//	DPRINTF(DEBUG_CLIENTS, "Doing reverse lookup for client %d IP %s\n",
	//			c->sock, addrbuf);

	struct hostent *he;
	if ((he = gethostbyaddr(&c->cliaddr.sin_addr, sizeof(struct sockaddr_in),
			AF_INET)) != NULL) {
		strncpy(c->hostname, he->h_name, MAX_HOSTNAME);
	} else {
		strncpy(c->hostname, addrbuf, MAX_HOSTNAME);
	}
	//	DPRINTF(DEBUG_CLIENTS, "Client %d name: %s\n", c->sock, c->hostname);
}

void accept_client(int sock, client_pool *p)
{
	int clisock;
	struct sockaddr_in cliaddr;
	//    socklen_t addrlen;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	client *c;

	//	DPRINTF(DEBUG_SOCKETS, "Accepting a connection\n");
	clisock = accept(sock, (struct sockaddr *)&cliaddr, &addrlen);
	if (clisock == -1) {
		perror("client accept failed from the http port");
		//		clienterror(c->sock, "GET", "501", "Not Implemented", "This method unimplemented");
		return;
	}
	if (fcntl(clisock, F_SETFL, O_NONBLOCK) == -1) {
		printf("could not set client socket to non-blocking for client %s",
				inet_ntoa(cliaddr.sin_addr));
		return;
	}

	// log into log file client socket info when client connects
	//	DPRINTF(DEBUG_SOCKETS|DEBUG_CLIENTS, "Got client on socket %d\n", clisock);

	c = new_client(p, clisock);
	c->cliaddr = cliaddr;
	c->ssl_connection = 0;
	set_hostname(c);
}

void disconnect_client(client *c, client_pool *p)
{
	//	DPRINTF(DEBUG_CLIENTS, "Disconnecting client %d\n", c->sock);
	//	proto_disconnect(c, p, NULL);

	//only disconnect if client sent a connection close request in http
	if (c->close_connection == DONT_CLOSE_CONN)
		return;
	//	printf("disconnecting client %d\n", c->sock);
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


	//	DPRINTF(DEBUG_INPUT, "Handling input from client %d\n", c->sock);
	memset(inbuf, 0, MAX_HEADER_LEN+1);
	memset(method, 0, 10);
	memset(uri, 0, MAXLINE);
	memset(version, 0, 12);

	// put in call for ssl read function to transfer data to inbuf
	if (c->ssl_connection == 1)
	{
		//read_from_ssl_socket(c, ssl_client_list, p, inbufptr);
		nread = read_from_ssl_client(c, p, inbufptr);
		//printf("out of ssl read in handle input: %s\n"
		//		"and numbytes = %d\n", inbufptr, nread);
		//read_from_ssl_socket()
	}
	else
	{
		nread = read(c->sock, inbuf, sizeof(inbuf)-1);
		inbufptr = inbuf;
		//printf("out of basic read in handle input: %s\n", inbufptr);
	}
	//printf("read %d bytes from client\n",nread);
	//printf("%sEND\n",inbuf);

	if (nread == 0)
	{
		disconnect_client(c, p);
		return;
	}
	if (nread == -1)
	{
		http_error( c, "Error from reading client socket", "500",
				"Internal Server Error",
				"An internal server error has occurred ", 1, 1);
		disconnect_client(c, p);
		return;
	}
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
	//	printf("Start Switch\n");
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
	default: break;
	}

	// we come here if the request is a fresh one (not a request that is too big
	// for the read buffer and thus got pipelined )
	//	printf("Skipped Switch\n");
	// now start parsing the first line of the request for correct HTTP
	// version and finding out what request method has been received
	request_line = strtok(inbuf, "\r\n");

	// debug output to show the http request
	// i = 0;
	//	while (request_line != NULL)
	//	{
	//		printf("Line %d : %s\n", i++, request_line);
	//		request_line = strtok(NULL, "\r\n");
	//	}

	num_matches = sscanf(request_line, "%s %s %s", method, uri, version);
	//printf("method %s uri %s version %s \n", method, uri, version);
	// check to see if we actually received a proper http request line
	if (num_matches != 3)
	{
		http_error( c, "Client has been disconnected from server", "400",
				"Bad Request",
				"broken HTTP request received", CLOSE_CONN, SEND_HTTP_BODY);
		disconnect_client(c, p);
		return;
	}


	// PUT BRANCH FOR CGI HANDLING HERE
	//	if (strstr(uri, "/cgi-bin/"))
	char *cgi_source;
	if (strlen(cla.cgi_folder) == 0)
		cgi_source = cla.cgi_script;
	else
		cgi_source = cla.cgi_folder;

	if (strncmp(uri, cgi_source, strlen(cgi_source)) == 0)
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
					"This server accepts HTTP requests of the following version only ", 1, 0);
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
				"This server accepts HTTP requests of the following version only ", 1, 1);
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
				"This method is unimplemented", 0, 1);
		return;
	}
	if (close_connection == 1)
	{
		printf("got a close connection");
		disconnect_client(c,p);
	}

	//printf("OK\n");
}

void log_into_file(char *message)
{
	FILE *fp;
	fp = fopen(cla.log_file,"a"); // append mode
	if( fp == NULL )
		return;
	fprintf(fp, "%s",message);
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
	case SIGTERM:
		/* finalize and shutdown the server */
		// TODO: liso_shutdown(NULL, EXIT_SUCCESS);
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
	char debug_output[MAXLINE]= {0};
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
		sprintf(debug_output,"Could not open lock file for lisod daemon");
		log_into_file(debug_output);
		exit(EXIT_FAILURE); /* can not open */
	}

	if (lockf(lfp, F_TLOCK, 0) < 0)
	{
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

	sprintf(debug_output,"Successfully daemonized lisod process, pid %d.", pid);
	log_into_file(debug_output);

	return EXIT_SUCCESS;
}


