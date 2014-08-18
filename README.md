web_server
==========

An basic implementation of  a web server which supports HTTP requests, HTTPS and CGI.

This was done as an assignment for my networks course (15-441) when i studied in 
Carnegie Mellon University (CMU). 

Since this is more of an assignment for learning, don't expect the server to be robust to 
errors. You are welcome to use this as a means to understand the inner workings of a web 
server though. I have tried to be as verbose as possible when it comes to documenting 
what my code does.

Start the program with  the following :
```
./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder> <CGI folder or script name> <private key file> <certificate file>
```
<dl>
  <dt>Arguments:</dt>
  <dd>HTTP port – the port for the HTTP (or echo) server to listen on</dd>
  <dd>HTTPS port – the port for the HTTPS server to listen on</dd>
  <dd>log file – file to send log messages to (debug, info, error)</dd>
  <dd>lock file – file to lock on when becoming a daemon process</dd>
  <dd>www folder – folder containing a tree to serve as the root of a website</dd>
  <dd>CGI folder or script name – folder containing CGI programs</dd>
  <dd>private key file – private key file path</dd>
  <dd>certificate file – certificate file path</dd>
</dl>
This project uses a set of linked list macros called [utlist](http://troydhanson.github.io/uthash/utlist.html)
Thsi project also has some code which is provided by the TAs of the 15-441 course 
