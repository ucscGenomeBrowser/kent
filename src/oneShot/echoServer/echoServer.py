#!/usr/bin/env/python
# echoServer - A little echo server to compare C vs. Python implementation. 

import sys
import time
import thread
from socket import *

def usage():
# Explain usage and exit.
    print "echoServer - a little echo server to compare C vs. Python implementation"
    print "usage:"
    print "   echoServer port"
    sys.exit(1)

def now():
# Return current time as a string.
    return time.ctime(time.time())

def echoThread(connection):
# Write out what we got, with 'Echo=>' prepended 
    time.sleep(5)
    while 1:
        data = connection.recv(1024)
	if not data: break
	connection.send('Echo=>' + data);
    connection.close()

def echoServer(port):
# echoServer - A little echo server to compare C vs. Python implementation. 
    sock = socket(AF_INET, SOCK_STREAM)
    sock.bind(('', int(port)))
    sock.listen(5)
    while 1:
	connection, address = sock.accept()
	print 'Server connected by', address, 'at', now()
	thread.start_new(echoThread, (connection,))

if len(sys.argv) <= 1:
    usage();
echoServer(sys.argv[1])
