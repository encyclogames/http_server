/*
 * handler.c
 *
 *      Author: Fahad Islam
 */
#include "handler.h"
#include "ssl_handler.h"

void write_to_client(client *c, char* buffer, int numbytes);

int handle_GET(client *c, char *uri, char* www_folder)
{
	//	printf("GET GOT (%s)\n",c->inbuf);
	char* index = "/";
	char* index_html = "/index.html";
	char* request_end;
	char* request_line;
	char* one;
	char* last_request_line_read;
	//int close_connection = 0;
	//char header_name[32],header_value[MAXLINE];
	char file_location[MAXLINE];
	memset(file_location,0,MAXLINE);
	char file_buffer[MAX_LEN];
	int len;

	request_end = strstr(c->inbuf, "\r\n\r\n");
	// first check to see whether we have an incomplete request or not
	if (request_end != NULL)
	{
		c->request_incomplete = 0;
	}
	// buffer does not contain request end
	else
	{
		if (c->inbuf_size == MAX_HEADER_LEN) // buffer is full)
		{
			// at this point we know we have an incomplete request
			c->request_incomplete = 1; // incomplete GET
			printf("INCOMPLETE GET REQUEST FOUND\n");
		}
		else
		{
			// buffer not full but no request end means broken http request
			http_error( c, "Client has been disconnected from server", "400",
					"Bad Request",
					"broken HTTP request received", CLOSE_CONN, SEND_HTTP_BODY);
			return 1;
		}
	}

	// if we had a previously incomplete request then we need to join the split
	// requests together to be able to parse them properly
	if (strlen(c->incomplete_request_buffer) != 0)
	{
		one = strstr(c->inbuf, "\r\n");
		*one = '\0';
		strcat(c->incomplete_request_buffer, c->inbuf);
		//printf("split line back again %s\n",c->incomplete_request_buffer);
		*one = '\r';

		if (strstr(c->incomplete_request_buffer,"Connection") != NULL)
		{
			if (strcmp(strstr(c->incomplete_request_buffer,":")+1,"close") == 0)
				c->close_connection = 1;
		}
	}

	request_line = strtok(c->inbuf, "\r\n");
	// parse each line for headers that need to be supported
	//	int i = 0;
	while (request_line != NULL)
	{
		last_request_line_read = request_line;
		if (strstr(request_line,"Connection") != NULL)
		{
			if (strcmp(strstr(request_line,":")+1,"close") == 0)
				c->close_connection = 1;
		}

		//printf("Line %d : %s\n", i++, request_line);
		//		sscanf(request_line, "%s %s", header_name, header_value);
		//		printf("Line %d : %s %s\n", i++, header_name, header_value);
		request_line = strtok(NULL, "\r\n");
	}
	// dont start processing the request since its incomplete
	if (c->request_incomplete == 1)
	{
		memset(c->incomplete_request_buffer, 0, MAXLINE);
		memcpy(c->incomplete_request_buffer,
				last_request_line_read,
				strlen(last_request_line_read));
		//		memcpy(c->inbuf,last_request_line_read,strlen(last_request_line_read));
		memset(c->inbuf, 0, sizeof(c->inbuf));
		c->inbuf_size = 0;
		return 0; //dont close connection
	}

	//*************************************************************************
	// at this point, we have finished parsing the complete GET request
	// in both pipelined and non-pipelined scenarios, so we start processing
	// the request

	//modify uri to point to index.html if GET requests a "/"
	if (strcmp(uri,index) == 0 || strcmp(uri,index_html)== 0)
	{
		sprintf(file_location, "%s%s", www_folder, index_html);
	}
	else
	{
		sprintf(file_location, "%s%s", www_folder, uri);
	}

	FILE *fp;
	//	printf("File location : %s\n", file_location);
	fp = fopen(file_location,"r"); // read mode
	if( fp == NULL )
	{
		//		printf("something not right %s\n", strerror(errno));
		http_error(c, uri, "404", "Not Found",
				"This file doesn't exist on this server", 0, 1);
		reset_client_buffers(c);
		return 0;
	}

	char header_str[MAX_HEADER_LEN];//, body[MAX_LEN];
	memset(header_str, 0 , MAX_HEADER_LEN);
	//memset(body, 0 , MAX_LEN);
	char *header_ptr = header_str;

	create_http_response_header(header_ptr, fp, file_location);
	write_to_client(c, header_str, strlen(header_str));
	//	printf("The GET response header is as follows: \n%s",header_str);

	// header done so send entity for body by reading file
	rewind(fp); //reset file pointer back to beginning of file
	//printf("The GET response as follows: \n%s",header);
	len = fread(file_buffer,1, sizeof(file_buffer),fp);
	while (len != 0)
	{
		write_to_client(c, file_buffer, len);
		len = fread(file_buffer,1, sizeof(file_buffer),fp);
	}
	//	printf("The contents of %s file are :\n", file_location);
	//	printf("%s\n",file_buffer);

	fclose(fp);

	// since client request is fulfilled, clear the client request buffer
	// for next request due to persistent connection

	reset_client_buffers(c);
	printf("GET OK for uri: %s\n", uri);
	char debug_output[MAXLINE];
	sprintf(debug_output,"GET OK for uri: %s\n", uri);
	log_into_file(debug_output);
	return c->close_connection;
}

int handle_HEAD(client *c, char *uri, char* www_folder)
{
	//	http_error(c, "HEAD", "501", "Not Implemented", "This method is unimplemented", 0, 0);
	//	return;
	printf("start handle HEAD\n");
	//	printf(c->inbuf);
	char* index = "/";
	char* index_html = "/index.html";
	char* request_end;
	char* request_line;
	char* one;
	char* last_request_line_read;
	//char header_name[32],header_value[MAXLINE];
	char file_location[MAXLINE];
	memset(file_location,0,MAXLINE);

	request_end = strstr(c->inbuf, "\r\n\r\n");
	// first check to see whether we have an incomplete request or not
	if (request_end != NULL)
	{
		c->request_incomplete = 0;
	}
	// buffer does not contain request end
	else
	{
		if (c->inbuf_size == MAX_HEADER_LEN) // buffer is full)
		{
			// at this point we know we have an incomplete request
			c->request_incomplete = 2; // incomplete HEAD
			printf("INCOMPLETE HEAD REQUEST FOUND\n");
		}
		else
		{
			// buffer not full but no request end means broken http request
			http_error( c, "Client has been disconnected from server", "400",
					"Bad Request",
					"broken HTTP request received", 1, 1);
			return 1;
		}
	}

	// if we had a previously incomplete request then we need to join the split
	// requests together to be able to parse them properly
	if (strlen(c->incomplete_request_buffer) != 0)
	{
		one = strstr(c->inbuf, "\r\n");
		*one = '\0';
		strcat(c->incomplete_request_buffer, c->inbuf);
		printf("split line back again %s\n",c->incomplete_request_buffer);
		*one = '\r';

		if (strstr(c->incomplete_request_buffer,"Connection") != NULL)
		{
			if (strcmp(strstr(c->incomplete_request_buffer,":")+1,"close") == 0)
				c->close_connection = 1;
		}
	}

	request_line = strtok(c->inbuf, "\r\n");
	// parse each line for headers that need to be supported
	int i = 0;
	while (request_line != NULL)
	{
		last_request_line_read = request_line;
		if (strstr(request_line,"Connection") != NULL)
		{
			if (strcasecmp(strstr(request_line,":")+2,"close") == 0)
				c->close_connection = 1;
		}

		printf("Line %d : %s %d\n", i++, request_line,c->close_connection);
		//		sscanf(request_line, "%s %s", header_name, header_value);
		//		printf("Line %d : %s %s\n", i++, header_name, header_value);
		request_line = strtok(NULL, "\r\n");
	}

	// dont start processing the request since its incomplete
	if (c->request_incomplete == 2)
	{
		printf("request is found incpmplee\n");
		memset(c->incomplete_request_buffer, 0, MAXLINE);
		memcpy(c->incomplete_request_buffer,
				last_request_line_read,
				strlen(last_request_line_read));
		//		memcpy(c->inbuf,last_request_line_read,strlen(last_request_line_read));
		memset(c->inbuf, 0, MAX_HEADER_LEN);
		c->inbuf_size = 0;
		return 0; //dont close connection
	}


	//*************************************************************************
	// at this point, we have finished parsing the complete HEAD request
	// in both pipelined and non-pipelined scenarios, so we start processing
	// the request

	//modify uri to point to index.html if HEAD requests a "/"
	if (strcmp(uri,index) == 0 || strcmp(uri,index_html)== 0)
	{
		sprintf(file_location, "%s%s", www_folder, index_html);
	}
	else
	{
		sprintf(file_location, "%s%s", www_folder, uri);
	}

	FILE *fp;
	fp = fopen(file_location,"r"); // read mode
	if( fp == NULL )
	{
		http_error(c, uri, "404", "Not Found",
				"This file doesn't exist on this server", 0, 1);
		reset_client_buffers(c);
		return 0;
	}

	char header_str[MAX_HEADER_LEN];
	memset(header_str, 0 , MAX_HEADER_LEN);
	char *header_ptr = header_str;

	create_http_response_header(header_ptr, fp, file_location);
	write_to_client(c, header_str, strlen(header_str));
	//	printf("The HEAD response header is as follows: \n%s",header_str);
	//printf("doen parsing\n");
	fclose(fp);

	reset_client_buffers(c);

	//	memset(c->inbuf,0,sizeof(c->inbuf));
	//	c->request_incomplete = 0;
	//	printf("HEAD OK");
	return c->close_connection;
}

int handle_POST(client *c, char *uri, char* www_folder)
{
	printf("POST DATA IN FUNCTION:\n%s\n",c->inbuf);
	http_error(c, "POST", "501", "Not Implemented", "This method is unimplemented", 0, 1);
	//	char debug_output[MAXLINE];
	int close_connection = 0;

	/* $begin serve_dynamic */
	//	void serve_dynamic(int fd, char *filename, char *cgiargs)
	//	{
	//	    char buf[MAXLINE], *emptylist[] = { NULL };
	//
	//	    /* Return first part of HTTP response */
	//	    sprintf(buf, "HTTP/1.0 200 OK\r\n");
	//	    Rio_writen(fd, buf, strlen(buf));
	//	    sprintf(buf, "Server: Tiny Web Server\r\n");
	//	    Rio_writen(fd, buf, strlen(buf));
	//
	//	    if (Fork() == 0) { /* child */
	//		/* Real server would set all CGI vars here */
	//		setenv("QUERY_STRING", cgiargs, 1);
	//		Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
	//		Execve(filename, emptylist, environ); /* Run CGI program */
	//	    }
	//	    Wait(NULL); /* Parent waits for and reaps child */
	//	}
	/* $end serve_dynamic */

	//	printf(c->inbuf);
	//	printf("\npost request end\n");

	//	char* index = "/";
	//	char* index_html = "/index.html";
	//	char* request_end;
	char* request_line;
	//	char close_connection = 0;
	//	char header_name[32],header_value[MAXLINE];
	char file_location[MAXLINE];
	memset(file_location,0,MAXLINE);
	//	char file_buffer[MAX_HEADER_LEN];
	int post_content_len = 0;

	//	request_end = strstr(c->inbuf, "\r\n\r\n");
	request_line = strtok(c->inbuf, "\r\n");
	//	//skip the first request line and start reading the headers
	request_line = strtok(NULL, "\r\n");
	//	printf("%s\n",uri);
	// parse each line for headers that need to be supported
	//	int i = 0;
	while (request_line != NULL)
	{
		if (strstr(request_line,"Content-Length") != NULL)
		{
			post_content_len = atoi(strstr(request_line,":")+1);
			printf("Content length got %d \n", post_content_len); //debug output
		}
		//Log into file
		//		sprintf(debug_output,"Line %d : %s\n", i++, request_line); //debug output
		//		log_into_file(debug_output);
		request_line = strtok(NULL, "\r\n");
	}

	// check for content length flag and if not send back http error
	//  411_LENGTH_REQUIRED
	if (post_content_len == 0)
	{
		http_error(c, uri, "411", "Length Required", "This POST request is missing Content-Length sent to ", 0, 1);
	}

	// get location of cgi script
	sprintf(file_location, ".%s%s", www_folder, uri);
	printf("LOCATION OF CGI SCRIPT:%s\n",file_location);

	//log into file
	//	sprintf(debug_output,"POST OK\n");
	//	log_into_file(debug_output);

	return close_connection;
}


/***** HTTP HEADER RESPONSE MAKER*****/
/////////////////////////////////////////////////////////////////////////////
/*
 * create_http_response_header - returns the HTTP header message to the caller function
 *
 * params:
 * header - 		char buffer in which the whole header will lie
 * file_location - 	location of the requested file on the server from which
 * 					this function will read and get details
 * 	return:
 * 	1 if the file is not found on the server,0 the header has been
 * 				successfully created
 */
void create_http_response_header(char *header, FILE *fp, char* filepath)
{
	time_t rawtime;
	struct timespec lmtime;
	struct tm *tm_date;
	struct tm *tm_last_modified;
	char time_str[256];
	char mime_type[32];
	int fd = fileno(fp);
	int length = 0;

	//create the header part of the response
	length = sprintf(header, "HTTP/1.1 200 OK\r\n");
	// NEW SAFER STRING COMBINING VERSION
	length += sprintf(header+length, "Server: Liso/1.0\r\n");
	length += sprintf(header+length, "Connection: keep-alive\r\n");

	time( &rawtime );
	tm_date = gmtime( &rawtime );
	memset(time_str,0,256);
	strftime(time_str,256,"%a, %e %b %Y %H:%M:%S %Z", tm_date);
	length += sprintf(header+length, "Date: %s\r\n",time_str);

	// now need content length, last modified
	struct stat file_stats;
	fstat(fd,&file_stats);
	lmtime = file_stats.st_mtim;
	tm_last_modified = gmtime( &lmtime.tv_sec );
	memset(time_str,0,256);
	strftime(time_str,256,"%a, %e %b %Y %H:%M:%S %Z", tm_last_modified);

	length += sprintf(header+length, "Last-Modified: %s\r\n",time_str);
	length += sprintf(header+length, "Content-Length: %lld\r\n",(long long) file_stats.st_size);
	//	printf("File size: %lld bytes\n",(long long) file_stats.st_size);
	//	printf("Last file modification:   %s", ctime(&file_stats.st_mtim)); //what in the world ??? mtim ???

	//   content type,
	get_mimetype(filepath, mime_type);
	length += sprintf(header+length, "Content-Type: %s\r\n",mime_type);

	//end response header with newline
	length += sprintf(header+length, "\r\n");


	// OLD UNSAFE STRING COMBINING VERSION VERSION
	//	sprintf(header, "%sServer: Liso/1.0\r\n",header);
	//	sprintf(header, "%sConnection: keep-alive\r\n",header);
	//
	//	time( &rawtime );
	//	tm_date = gmtime( &rawtime );
	//	memset(time_str,0,256);
	//	strftime(time_str,256,"%a, %e %b %Y %H:%M:%S %Z", tm_date);
	//	sprintf(header, "%sDate: %s\r\n",header, time_str);
	//
	//	// now need content length, last modified
	//	struct stat file_stats;
	//	fstat(fd,&file_stats);
	//	lmtime = file_stats.st_mtim;
	//	tm_last_modified = gmtime( &lmtime.tv_sec );
	//	memset(time_str,0,256);
	//	strftime(time_str,256,"%a, %e %b %Y %H:%M:%S %Z", tm_last_modified);
	//	sprintf(header, "%sLast-Modified: %s\r\n",header, time_str);
	//	sprintf(header, "%sContent-Length: %lld\r\n",header, (long long) file_stats.st_size);
	//	//	printf("File size: %lld bytes\n",(long long) file_stats.st_size);
	//	//	printf("Last file modification:   %s", ctime(&file_stats.st_mtim)); //what in the world ??? mtim ???
	//
	//	//   content type,
	//	get_mimetype(filepath, mime_type);
	//	sprintf(header, "%sContent-Type: %s\r\n",header, mime_type);
	//
	//	//end response header with newline
	//	sprintf(header, "%s\r\n",header);
}

/*
 * get_mimetype - gets the MIME type of the file asked for from the uri to be
 * used for Content-Type header in the server response
 */
void get_mimetype(char *filepath, char *mimetype)
{
	if (strstr(filepath, ".html"))
		strcpy(mimetype, "text/html");
	else if (strstr(filepath, ".css"))
		strcpy(mimetype, "text/css");
	else if (strstr(filepath, ".txt"))
		strcpy(mimetype, "text/plain");
	else if (strstr(filepath, ".png"))
		strcpy(mimetype, "image/png");
	else if (strstr(filepath, ".gif"))
		strcpy(mimetype, "image/gif");
	else if (strstr(filepath, ".jpg"))
		strcpy(mimetype, "image/jpeg");
	else if (strstr(filepath, ".pdf"))
		strcpy(mimetype, "application/pdf");
	else
		strcpy(mimetype, "application/octet-stream");
}

/*
 * reset_client_buffers - clears the buffers containing old data of the client
 * in order to prepare for the next incoming request
 */
void reset_client_buffers(client *c)
{
	c->request_incomplete = 0;
	c->inbuf_size = 0;
	memset(c->inbuf, 0, sizeof(c->inbuf));
	memset(c->incomplete_request_buffer, 0,
			sizeof(c->incomplete_request_buffer));
}


/***** HTTP ERROR RESPONSE MAKER*****/
/////////////////////////////////////////////////////////////////////////////
/*
 * http_error - returns an error message to the client
 *
 * params:
 * fd - 		file descriptor/socket of the client
 * cause -		what is causing this error message
 * errnum -		the server response status code (eg: 501)
 * shortmsg -	the server pre-defined response message (eg: Not Implemented)
 * longmsg -	a sentence describing what went wrong
 * connection - a flag telling whether to close the connection or not.
 * 				0 will do nothing, 1 will include a Connection:Close in the
 * 				response and closes the client socket if its open
 * send_body -  will include an HTML entity specifying the reasoning for the
 * 				http error which has occurred (it is not sent for a HEAD
 * 				request). 0 does not include the entity and 1 will include it.
 */
void http_error(client *c, char *cause, char *errnum,
		char *shortmsg, char *longmsg, char connection, char send_body)
{
	int fd = c->sock;
	char buf[MAX_HEADER_LEN], body[MAX_HEADER_LEN];

	/* Build the HTTP response body */
	sprintf(body, "<html><title>My Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>Lisod Web Server by Fahad Islam</em>\r\n", body);

	/* Create the HTTP response header */
	sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
	sprintf(buf, "%sContent-type: text/html\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, (int)strlen(body));
	if (connection == 1)
		sprintf(buf, "%sConnection: close\r\n", buf);
	sprintf(buf, "%s\r\n", buf);

	if (c->ssl_connection == 0)
	{
		write(fd, buf, strlen(buf));
		if (send_body == 1)
			write(fd, body, strlen(body));
	}
	else
	{
		write_to_ssl_client(c, buf, strlen(buf));
		if (send_body == 1)
		{
			write_to_ssl_client(c, body, strlen(body));
		}
	}
	// remember to close the client socket
}


void write_to_client(client *c, char* buffer, int numbytes)
{
	if (c->ssl_connection == 0)
	{
		write(c->sock, buffer, numbytes);
	}
	else
	{
		write_to_ssl_client(c, buffer, numbytes);
	}
}
