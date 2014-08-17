#!/usr/bin/env python
#
# This script serves as a simple TLSv1 client for 15-441
#
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,
#          Charles Rang <rang972@gmail.com>,
#          Wolfgang Richter <wolf@cs.cmu.edu>

import pprint
import socket
import ssl

# try a connection
sock = socket.create_connection(('localhost', 4443))  #4949 is old port
#tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
#                            ca_certs='../certs/signer.crt',
#                            ssl_version=ssl.PROTOCOL_TLSv1)
pprint.pprint('start conn wrap on python socket');

tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
                            ca_certs='./myca.crt',
                            ssl_version=ssl.PROTOCOL_TLSv1)

# what cert did he present?
pprint.pprint('connected so going to try to get cert DETAILS');
pprint.pprint(tls.getpeercert())

#tls.sendall('this is a test message')
tls.sendall('GET / HTTP/1.1\r\n\r\n')
pprint.pprint(tls.recv(4096).split("\r\n"))
pprint.pprint(tls.recv(4096))
tls.close()



# try another connection!
sock = socket.create_connection(('localhost', 4443))
#tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
#                            ca_certs='../certs/signer.crt',
#                            ssl_version=ssl.PROTOCOL_TLSv1)
tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
                            ca_certs='./myca.crt',
                            ssl_version=ssl.PROTOCOL_TLSv1)

#tls.sendall('this is a test message!!!')


tls.sendall('GET /www/cgi/adder.cgi?m=5&n=6 HTTP/1.1\r\n\r\n')
pprint.pprint(tls.recv(4096).split("\r\n"))
pprint.pprint(tls.recv(4096))
tls.close()



sock = socket.create_connection(('localhost', 4443))
#tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
#                            ca_certs='../certs/signer.crt',
#                            ssl_version=ssl.PROTOCOL_TLSv1)
tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
                            ca_certs='./myca.crt',
                            ssl_version=ssl.PROTOCOL_TLSv1)
tls.sendall('POST /www/cgi/collect.cgi HTTP/1.1\r\n'
'Host: 127.0.0.1:4443\r\n'
'Connection: keep-alive\r\n'
'Content-Length: 29\r\n'
'Cache-Control: max-age=0\r\n'
'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n'
'Origin: https://127.0.0.1:4443\r\n'
'User-Agent: Mozilla/5.0 (X11; Linux i686) AppleWebKit/537.36 (KHTML, like Gecko) Ubuntu Chromium/36.0.1985.125 Chrome/36.0.1985.125 Safari/537.36\r\n'
'Content-Type: application/x-www-form-urlencoded\r\n'
'Referer: https://127.0.0.1:4443/\r\n'
'Accept-Encoding: gzip,deflate,sdch\r\n'
'Accept-Language: en-US,en;q=0.8\r\n'
'\r\n'
'data=Testing+you+chromium+ssl')
pprint.pprint(tls.recv(4096).split("\r\n"))
pprint.pprint(tls.recv(4096))
tls.close()

exit(0)
