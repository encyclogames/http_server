web_server
==========

An basic implementation of  a web server which supports HTTP requests and CGI. 

Since this is more of a project for learning, don't expect the server to be robust to 
errors. You are welcome to use this as a means to understand the inner workings of a web 
server though. I have tried to be as verbose as possible when it comes to documenting 
what my code does.

Start the program with  the following :
```
./main <HTTP port> <log file> <www folder> <CGI folder or script name>
```
<dl>
  <dt>Arguments:</dt>
  <dd>HTTP port – the port for the HTTP (or echo) server to listen on</dd>
  <dd>log file – file to send log messages to (debug, info, error)</dd>
  <dd>www folder – folder containing a tree to serve as the root of a website</dd>
  <dd>CGI folder or script name – folder containing CGI programs</dd>
</dl>

This project uses a set of linked list macros called [utlist](http://troydhanson.github.io/uthash/utlist.html)
