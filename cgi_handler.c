/*
 * cgi_handler.c
 *
 *      Author: Fahad Islam
 */


#include "cgi_handler.h"

extern char **environ;

/**************** BEGIN CONSTANTS ***************/
#define FILENAME_G "./cgi_script.py"
#define BUF_SIZE 4096

/* note: null terminated arrays (null pointer) */
//char* ARGV_G[] = {
//		FILENAME_G,
//		NULL
//};
//
//char* ENVP_G[] = {
//		/*1*/   "CONTENT_LENGTH=",
//		/*2*/	"CONTENT-TYPE=",
//		/*3*/	"GATEWAY_INTERFACE=CGI/1.1",
//		/*4*/	"PATH_INFO=",
//		/*5*/	"QUERY_STRING=action=opensearch&search=HT&namespace=0&suggest=",
//		/*6*/	"REMOTE_ADDR=128.2.215.22",
//		/*7*/	"REMOTE_HOST=gs9671.sp.cs.cmu.edu", // not needed, so might remove it
//		/*8*/	"REQUEST_METHOD=GET",
//		/*9*/	"REQUEST_URI=",
//		/*10*/	"SCRIPT_NAME=/w/api.php",
//		/*11*/	"SERVER_NAME=",
//		/*12*/	"SERVER_PORT=80",
//		/*13*/	"SERVER_PROTOCOL=HTTP/1.1",
//		/*14*/	"SERVER_SOFTWARE=Liso/1.0",
//		/*15*/	"HOST_NAME=en.wikipedia.org",
//		/*16*/	"HTTP_ACCEPT=/application/json, text/javascript, */*; q=0.01",
//		/*17*/	"HTTP_REFERER=http://en.wikipedia.org/w/index.php?title=Special%3ASearch&search=test+wikipedia+search",
//		/*18*/	"HTTP_ACCEPT_ENCODING=gzip,deflate,sdch",
//		/*19*/	"HTTP_ACCEPT_LANGUAGE=en-US,en;q=0.8",
//		/*20*/	"HTTP_ACCEPT_CHARSET=ISO-8859-1,utf-8;q=0.7,*;q=0.3",
//		/*21*/	"HTTP_COOKIE=clicktracking-session=v7JnLVqLFpy3bs5hVDdg4Man4F096mQmY; mediaWiki.user.bucket%3Aext.articleFeedback-tracking=8%3Aignore; mediaWiki.user.bucket%3Aext.articleFeedback-options=8%3Ashow; mediaWiki.user.bucket:ext.articleFeedback-tracking=8%3Aignore; mediaWiki.user.bucket:ext.articleFeedback-options=8%3Ashow",
//		/*22*/	"HTTP_USER_AGENT=Mozilla/5.0 (X11; Linux i686) AppleWebKit/535.1 (KHTML, like Gecko) Chrome/14.0.835.186 Safari/535.1",
//		/*23*/	"HTTP_CONNECTION=keep-alive",
//		/*24*/	"HTTP_HOST=en.wikipedia.org",
//		/*25*/NULL
//};
/**************** END CONSTANTS ***************/



//////////// SELF CODE


int handle_cgi_request(client *c, char *uri)
{
	char *request_end = NULL;
	char *message_body = NULL;
	char *message_body_buffer = NULL;
	char *inbufptr = NULL;
	int content_length = 0, request_length = 0, nread = 0, ret_val = 0;
	int received_message_body_length = 0, bytes_to_read = 0;
		printf("cgi client inbuf:%sEND\n", c->inbuf);
//		printf("uri:%sEND\n", uri);

	request_end = strstr(c->inbuf, "\r\n\r\n");
	// first check to see whether we have an incomplete request or not
	if (request_end != NULL)
	{
		c->request_incomplete = 0;
		message_body = strstr(c->inbuf, "\r\n\r\n") + 4;
	}
	// buffer does not contain "\r\n\r\n"
	else
	{
		http_error( c, "Client has been disconnected from server", "400",
				"Bad Request",
				"broken HTTP request received", CLOSE_CONN, SEND_HTTP_BODY);
		return 1;
	}

	if (set_env_vars(c, uri) == -1)
		return 1;

	content_length = set_http_env_vars(c);

	// START message body buffering
	if (content_length != 0)
	{
		printf("full size:%d\nreq len : %d\n", c->inbuf_size, message_body - c->inbuf);
		request_length = message_body - c->inbuf;
		received_message_body_length = c->inbuf_size - request_length;
		printf("mes bdy len : %d\n", received_message_body_length);

		// prepare message body into new buffer to be written to exec child
		message_body_buffer = malloc(content_length);
		if (message_body_buffer == NULL)
		{
			http_error( c, "Error in mallocing msg_body_buffer", "500",
					"Internal Server Error",
					"An internal server error has occurred ",
					CLOSE_CONN, SEND_HTTP_BODY);
			return 1;
		}
		memset(message_body_buffer, 0, content_length);
		memcpy(message_body_buffer, message_body, received_message_body_length);

		if ( received_message_body_length < content_length)
		{
			// the client buffer does not have the complete message body
			// so we read the remaining bytes from the client socket
			bytes_to_read = content_length - received_message_body_length;
			c->inbuf_size = 0;
			memset(c->inbuf, 0, sizeof(c->inbuf));
			inbufptr = c->inbuf;

			if (c->ssl_connection == 1)
			{
				nread = read_from_ssl_client(c, inbufptr, bytes_to_read);
				memcpy(message_body_buffer + received_message_body_length,
						c->inbuf, nread);
				//printf("out of ssl read in cgi handler: %sEND\n", inbufptr);
			}
			else
			{
				// keep reading until we get the whole message body
				while (nread < bytes_to_read)
				{
					nread = read(c->sock, c->inbuf, bytes_to_read);
					if (nread == -1)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							nread = 0;
							continue;
						}
						else
						{
							http_error( c, "Error in reading message body from client",
									"500", "Internal Server Error",
									"An internal server error has occurred ",
									CLOSE_CONN, SEND_HTTP_BODY);
							return 1;
						}

					}
					memcpy(message_body_buffer + received_message_body_length,
							c->inbuf, nread);
					received_message_body_length += nread;
					bytes_to_read -= nread;
					nread = 0; // reset for next read
					inbufptr = c->inbuf;
				}
				//printf("out of basic read in cgi handler: %sEND\n", inbufptr);
			}
		}
	} // END message body buffering

	cgi_client* cgiobj = new_cgi_client(c->sock);
	if (cgiobj == NULL)
	{
		http_error( c, "Error in setting up cgi struct backend", "500",
				"Internal Server Error",
				"An internal server error has occurred ",
				DONT_CLOSE_CONN, SEND_HTTP_BODY);
		return 1;
	}
	LL_APPEND(cgi_client_list, cgiobj);

	// Setup/exec child cgi using the cgi client obj
	ret_val = cgi_child_process_creator(c, message_body_buffer, content_length, cgiobj);

	if (ret_val == -1)
	{
		http_error( c, "CGI script exec failed", "500",
				"Internal Server Error",
				"An internal server error has occurred ",
				DONT_CLOSE_CONN, SEND_HTTP_BODY);
		return 1;
	}

	return 0;
}

int set_env_vars(client *c, char *uri)
{
	setenv("GATEWAY_INTERFACE","CGI/1.1",1);
	setenv("SERVER_PROTOCOL","HTTP/1.1",1);
	setenv("SERVER_SOFTWARE","Liso/1.0",1);

	// we cannot set env vars directly to an int since they expect char*
	// so i make a char* variable that will take the typecast version of
	// ints to a char and then set the env var to the typecast string
	char env_var_typecast[10];
	memset(env_var_typecast, 0, 10);
	if (c->ssl_connection == 1)
	{
		sprintf (env_var_typecast, "%d", cla.https_port);
		setenv("SERVER_PORT",env_var_typecast,1);
	}
	else
	{
		sprintf (env_var_typecast, "%d", cla.http_port);
		setenv("SERVER_PORT",env_var_typecast,1);
	}

	char *client_ip = inet_ntoa(c->cliaddr.sin_addr);
	setenv("REMOTE_ADDR",client_ip,1);

	if (strncmp(c->inbuf, "GET ", 4) == 0) // space after GET is necessary
		setenv("REQUEST_METHOD","GET",1);
	else if (strncmp(c->inbuf, "HEAD", 4) == 0)
		setenv("REQUEST_METHOD","HEAD",1);
	else if (strncmp(c->inbuf, "POST", 4) == 0)
		setenv("REQUEST_METHOD","POST",1);
	else
	{
		char *space_after_method = strstr(c->inbuf, " ");
		*space_after_method = '\0';
		// at this point, c->inbuf only has method as a string in it
		http_error(c, c->inbuf, "501", "Not Implemented",
				"This server only handles GET,HEAD and POST requests. "
				"This method is unimplemented", DONT_CLOSE_CONN, SEND_HTTP_BODY);
		return -1;
	}

	int ret = set_env_vars_from_uri(uri);
	if (ret == -1)
	{
		http_error( c, uri, "404",
				"Not Found",
				"the CGI resource could not be located from uri",
				DONT_CLOSE_CONN, SEND_HTTP_BODY);
		return -1;
	}
	//	char *x;
	//	/* set environment variable _EDC_ANSI_OPEN_DEFAULT to "Y" */
	//	setenv("_EDC_ANSI_OPEN_DEFAULT","Y",1);
	//	/* set x to the current value of the _EDC_ANSI_OPEN_DEFAULT*/
	//	x = getenv("_EDC_ANSI_OPEN_DEFAULT");
	return 0;
}

int set_env_vars_from_uri(char *uri)
{
	printf("folder:%s script:%s\n", cla.cgi_folder, cla.cgi_script);

	// keep these uris as tests for parsing and move them to a python test later
	//	 char uri[] = "http://yourserver/www/cgi/search.cgi/misc/movies.mdb?sgi";
	// char uri[] = "http://localhost:8080/examples/www/cgi/HelloWorld/hello+there/fred";
	char uri_copy[MAXLINE] = "";
	char *path_info;
	char *script_name = NULL;
	strcpy(uri_copy, uri);

	char *query_string = strstr(uri, "?");
	if (query_string != NULL)
	{
		// skip the question mark char indicating start of query string
		setenv("QUERY_STRING",query_string+1,1);
		*query_string = '\0';
	}
	else
	{
		// clear query string for current cgi request since it might have
		// existing info set in it from the previous handled cgi request
		unsetenv("QUERY_STRING");
	}

	setenv("REQUEST_URI",uri,1);
	script_name = strstr(uri, "/cgi/");
	if (script_name == NULL)
	{
		// uri does not have proper cgi script location
		// so cannot proceed with request
		return -1;
	}
	else
	{
		path_info = strstr(script_name+strlen("/cgi/")+1, "/");
		if (path_info == NULL)
		{
			unsetenv("PATH_INFO");
		}
		else
		{
			setenv("PATH_INFO",path_info,1);
			*path_info = '\0';
		}
		setenv("SCRIPT_NAME",script_name,1);
	}

	return 0;
}

int set_http_env_vars(client *c)
{
	char *request_line;
	int content_length = 0;
	//	char *message_body = strstr(c->inbuf, "\r\n\r\n")+4;
	request_line = strtok(c->inbuf, "\r\n");
	// parse each line for headers that need to be supported
	//		int i = 0;
	while (request_line != NULL)
	{
		//		printf("iter %d\n",i++);
		if (strstr(request_line,"Connection:") != NULL)
		{
			if (strcmp(strstr(request_line,":")+2,"close") == 0)
			{
				c->close_connection = 1;
			}
			setenv("HTTP_CONNECTION",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"Accept:") != NULL)
		{
			setenv("HTTP_ACCEPT",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"Referer:") != NULL)
		{
			setenv("HTTP_REFERER",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"Accept-Encoding:") != NULL)
		{
			setenv("HTTP_ACCEPT_ENCODING",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"Accept-Language:") != NULL)
		{
			setenv("HTTP_ACCEPT_LANGUAGE",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"Accept-Charset:") != NULL)
		{
			setenv("HTTP_ACCEPT_CHARSET",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"Host:") != NULL)
		{
			setenv("HTTP_HOST",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"Cookie:") != NULL)
		{
			setenv("HTTP_COOKIE",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"User-Agent:") != NULL)
		{
			setenv("HTTP_USER_AGENT",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"Content-Type:") != NULL)
		{
			setenv("CONTENT_TYPE",strstr(request_line,":")+2,1);
		}
		else if (strstr(request_line,"Content-Length:") != NULL)
		{
			setenv("CONTENT_LENGTH",strstr(request_line,":")+2,1);
			content_length =  atoi(strstr(request_line,":")+2);
			printf("GOT CL:%d\n",content_length);
		}

		//		printf("Line %d : %s\n", i++, request_line);
		request_line = strtok(NULL, "\r\n");
	}


	return content_length;
}

cgi_client *
new_cgi_client(int sock)
{
	cgi_client *nc = NULL;

	nc = malloc(sizeof(cgi_client));

	if (nc == NULL)
	{
		printf("malloc failed for cgi_client\n");
		return nc;
	}
	memset(nc, 0, sizeof(cgi_client));
	nc->child_pid = 0;
	nc->client_sock = sock;
	memset(nc->pipe_parent2child, 0, 2 * sizeof(int));
	memset(nc->pipe_child2parent, 0, 2 * sizeof(int));
	nc->next = NULL;

	if (pipe(nc->pipe_parent2child) < 0)
	{
		fprintf(stderr, "Error creating pipe for parent2child.\n");
		free(nc);
		return NULL;
	}

	if (pipe(nc->pipe_child2parent) < 0)
	{
		fprintf(stderr, "Error creating pipe for child2parent.\n");
		free(nc);
		return NULL;
	}

	return nc;
}

///////////// END SELF CODE



/**************** BEGIN UTILITY FUNCTIONS ***************/
/* error messages stolen from: http://linux.die.net/man/2/execve
 * This function was implemented by the course TAs and is provided to us
 * */
void execve_error_handler()
{
	switch (errno)
	{
	case E2BIG:
		fprintf(stderr, "The total number of bytes in the environment \
(envp) and argument list (argv) is too large.\n");
		return;
	case EACCES:
		fprintf(stderr, "Execute permission is denied for the file or a \
script or ELF interpreter.\n");
		return;
	case EFAULT:
		fprintf(stderr, "filename points outside your accessible address \
space.\n");
		return;
	case EINVAL:
		fprintf(stderr, "An ELF executable had more than one PT_INTERP \
segment (i.e., tried to name more than one \
interpreter).\n");
		return;
	case EIO:
		fprintf(stderr, "An I/O error occurred.\n");
		return;
	case EISDIR:
		fprintf(stderr, "An ELF interpreter was a directory.\n");
		return;
	case ELIBBAD:
		fprintf(stderr, "An ELF interpreter was not in a recognised \
format.\n");
		return;
	case ELOOP:
		fprintf(stderr, "Too many symbolic links were encountered in \
resolving filename or the name of a script \
or ELF interpreter.\n");
		return;
	case EMFILE:
		fprintf(stderr, "The process has the maximum number of files \
open.\n");
		return;
	case ENAMETOOLONG:
		fprintf(stderr, "filename is too long.\n");
		return;
	case ENFILE:
		fprintf(stderr, "The system limit on the total number of open \
files has been reached.\n");
		return;
	case ENOENT:
		fprintf(stderr, "The file filename or a script or ELF interpreter \
does not exist, or a shared library needed for \
file or interpreter cannot be found.\n");
		return;
	case ENOEXEC:
		fprintf(stderr, "An executable is not in a recognised format, is \
for the wrong architecture, or has some other \
format error that means it cannot be \
executed.\n");
		return;
	case ENOMEM:
		fprintf(stderr, "Insufficient kernel memory was available.\n");
		return;
	case ENOTDIR:
		fprintf(stderr, "A component of the path prefix of filename or a \
script or ELF interpreter is not a directory.\n");
		return;
	case EPERM:
		fprintf(stderr, "The file system is mounted nosuid, the user is \
not the superuser, and the file has an SUID or \
SGID bit set.\n");
		return;
	case ETXTBSY:
		fprintf(stderr, "Executable was open for writing by one or more \
processes.\n");
		return;
	default:
		fprintf(stderr, "Unkown error occurred with execve().\n");
		return;
	}
}
/**************** END UTILITY FUNCTIONS ***************/




//int run_main(int argc, char * argv[])
//{
//	/*************** BEGIN VARIABLE DECLARATIONS **************/
//	pid_t pid;
//	int stdin_pipe[2];
//	int stdout_pipe[2];
//	char buf[BUF_SIZE];
//	int readret;
//	/*************** END VARIABLE DECLARATIONS **************/
//
//	/*************** BEGIN PIPE **************/
//	/* 0 can be read from, 1 can be written to */
//	if (pipe(stdin_pipe) < 0)
//	{
//		fprintf(stderr, "Error piping for stdin.\n");
//		return EXIT_FAILURE;
//	}
//
//	if (pipe(stdout_pipe) < 0)
//	{
//		fprintf(stderr, "Error piping for stdout.\n");
//		return EXIT_FAILURE;
//	}
//	/*************** END PIPE **************/
//
//	/*************** BEGIN FORK **************/
//	pid = fork();
//	/* not good */
//	if (pid < 0)
//	{
//		fprintf(stderr, "Something really bad happened when fork()ing.\n");
//		return EXIT_FAILURE;
//	}
//
//	/* child, setup environment, execve */
//	if (pid == 0)
//	{
//		/*************** BEGIN EXECVE ****************/
//		close(stdout_pipe[0]);
//		close(stdin_pipe[1]);
//		dup2(stdout_pipe[1], fileno(stdout));
//		dup2(stdin_pipe[0], fileno(stdin));
//		/* you should probably do something with stderr */
//
//		/* pretty much no matter what, if it returns bad things happened... */
//		if (execve(FILENAME, ARGV, ENVP))
//		{
//			execve_error_handler();
//			fprintf(stderr, "Error executing execve syscall.\n");
//			return EXIT_FAILURE;
//		}
//		/*************** END EXECVE ****************/
//	}
//
//	if (pid > 0)
//	{
//		fprintf(stdout, "Parent: Heading to select() loop.\n");
//		close(stdout_pipe[1]);
//		close(stdin_pipe[0]);
//
//		if (write(stdin_pipe[1], POST_BODY, strlen(POST_BODY)) < 0)
//		{
//			fprintf(stderr, "Error writing to spawned CGI program.\n");
//			return EXIT_FAILURE;
//		}
//
//		close(stdin_pipe[1]); /* finished writing to spawn */
//
//		/* you want to be looping with select() telling you when to read */
//		while((readret = read(stdout_pipe[0], buf, BUF_SIZE-1)) > 0)
//		{
//			buf[readret] = '\0'; /* nul-terminate string */
//			fprintf(stdout, "Got from CGI: %s\n", buf);
//		}
//
//		close(stdout_pipe[0]);
//		close(stdin_pipe[1]);
//
//		if (readret == 0)
//		{
//			fprintf(stdout, "CGI spawned process returned with EOF as expected.\n");
//			return EXIT_SUCCESS;
//		}
//	}
//	/*************** END FORK **************/
//
//	fprintf(stderr, "Process exiting, badly...how did we get here!?\n");
//	return EXIT_FAILURE;
//}


int cgi_child_process_creator(client *c, char *message_body,
		int content_length, cgi_client *cgi_client_struct)
{

	/*************** BEGIN FORK **************/
	cgi_client_struct->child_pid = fork();
	/* not good */
	if (cgi_client_struct->child_pid < 0)
	{
		fprintf(stderr, "Something really bad happened when fork()ing.\n");
		LL_DELETE(cgi_client_list, cgi_client_struct);
		return -1;
	}

	/* child, setup environment, execve */
	if (cgi_client_struct->child_pid == 0)
	{
		/*************** BEGIN EXECVE ****************/
		close(cgi_client_struct->pipe_child2parent[READ_END]);
		close(cgi_client_struct->pipe_parent2child[WRITE_END]);
		dup2(cgi_client_struct->pipe_child2parent[WRITE_END], fileno(stdout));
		dup2(cgi_client_struct->pipe_parent2child[READ_END], fileno(stdin));
		/* you should probably do something with stderr */

		char script_name[MAX_FILENAME_LEN];
		memset(script_name, 0, MAX_FILENAME_LEN);
		char *script_name_env_ptr = getenv("SCRIPT_NAME");
		strcpy(script_name, script_name_env_ptr);
		//		fprintf(stderr, "in child script name from env %s\n", script_name);
		if (script_name == NULL)
		{
			log_into_file( "Script name setting failed from env");
			LL_DELETE(cgi_client_list, cgi_client_struct);
			exit(EXIT_FAILURE);
		}
		char filename[MAX_FILENAME_LEN];
		memset(filename, 0, MAX_FILENAME_LEN);

		if (strlen(cla.cgi_script) != 0)
		{
			strcpy(filename, cla.cgi_script);
		}
		else
		{
			strcat(filename, cla.cgi_folder);
			script_name_env_ptr = script_name;
			if (filename[strlen(filename)-1] == '/')
			{
				script_name_env_ptr += 5; //skip the "/cgi/" in script name
			}
			else
			{
				script_name_env_ptr += 4; //skip the "/cgi" in script name
			}
			//			fprintf(stderr, "edited %s\n", script_name_env_ptr);
			strcat(filename, script_name_env_ptr);
		}
		char* argv_child[] = {filename,NULL};

		/* pretty much no matter what, if it returns bad things happened... */
		fprintf(stderr, "RUNNING file %s\n", filename);
		if (execve(filename, argv_child, environ))
		{
			execve_error_handler();
			log_into_file("Error executing execve syscall.\n");
			log_into_file(script_name_env_ptr);
			return -1;
		}
		/*************** END EXECVE ****************/
	}

	// parent
	if (cgi_client_struct->child_pid > 0)
	{
		fprintf(stdout, "Parent: Heading to select() loop.\n");
		close(cgi_client_struct->pipe_child2parent[WRITE_END]);
		close(cgi_client_struct->pipe_parent2child[READ_END]);

		if (write(cgi_client_struct->pipe_parent2child[WRITE_END],
				message_body, content_length) < 0)
		{
			fprintf(stderr, "Error writing to spawned CGI program.\n");
			return -1;
		}

		printf("finished writing message body.\n");

		/* finished writing to spawn */
		close(cgi_client_struct->pipe_parent2child[WRITE_END]);

		// return success so the child read fd
		return 0;

	}
	/*************** END FORK **************/

	fprintf(stderr, "Process exiting, badly...how did we get here!?\n");
	return -1;
}

void transfer_response_from_cgi_to_client(cgi_client *temp_cgi, client_pool *p)
{
	client *c = p->clients[temp_cgi->client_sock];
	//	printf("got it from client pool using cgi obj\n");

	char buf[MAX_LEN];
	int readret, status;
	// flag to check if read is the first from child or not
	char read_start = 1;
	char response[] = "HTTP/1.1 200 OK\r\n";
	//	char response_end = "";

	while((readret = read(temp_cgi->pipe_child2parent[READ_END],
			buf, MAX_LEN-1)) > 0)
	{
		if (read_start == 1) // 1st read from child valid, respond with 200
		{
			if (c->ssl_connection == 0)
			{
				write(temp_cgi->client_sock, response, strlen(response));
			}
			else
			{
				write_to_ssl_client(c, response, strlen(response));
			}
			read_start = 0;
		}
		buf[readret] = '\0'; /* nul-terminate string */
		//		fprintf(stdout, "Got from CGI: %s\n", buf);
		if (c->ssl_connection == 0)
		{
			write(temp_cgi->client_sock, buf, strlen(buf));
		}
		else
		{
			write_to_ssl_client(c, buf, strlen(buf));
		}
	}
	//	printf("CGI readert finished with %d.\n", readret);

	close(temp_cgi->pipe_child2parent[READ_END]);
	close(temp_cgi->pipe_parent2child[WRITE_END]);

	if (readret == -1)
	{
		// child read pipe broken at the very beginning
		// server responds with 500
		if (read_start == 1)
		{
			http_error( c, "Error in reading message response from child",
					"500", "Internal Server Error",
					"An internal server error has occurred ",
					CLOSE_CONN, SEND_HTTP_BODY);
		}
		// child error, so clean up child info in main server

	}

	if (readret == 0)
	{
		fprintf(stdout, "CGI spawned process returned with EOF as expected.\n");
		//		waitpid(temp_cgi->child_pid, &status, WNOHANG);
		//		status = WEXITSTATUS(status);

		waitpid(temp_cgi->child_pid, &status, WNOHANG);
		//		printf("waited on child after transfer finished\n");
		LL_DELETE(cgi_client_list, temp_cgi);
		free(temp_cgi);
	}

	// TO DO: check code for faulty read/broken pipe/incomplete child response
}

