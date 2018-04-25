#!/usr/bin/env python2
#
#    Client for testing CTcpServer class
#
#    Author      : Dmitry Piotrovsky for Percepto
#
#    interpreter: python 2.7


import socket
import sys
from time import sleep

TEST_HOST = "127.0.0.1"
TEST_PORT = 4444
PROTOVERSION = 0x01
OPCODE = 0x5a  # test operation
LEN_HI_OFFSET = 2
LEN_LO_OFFSET = 3
    
BLOCKCOUNT = 1000


# one more global loop for make continuous traffic
while True:
    # delay before new "BLOCKCOUNT" blocks send 
    sleep(0.5) # delay commented to simulate high traffic
    count = BLOCKCOUNT
#printouts should be disabled to simulate high traffic per second    
    while count > 0:
        count -= 1
        try:
            # Create a TCP/IP socket
            # New socket for new connection
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(3.0) 
            # Connect the socket to the port where the server is listening
            server_address = (TEST_HOST, TEST_PORT)
            print >>sys.stderr, 'Connecting to %s port %s' % server_address
            sock.connect(server_address)
            # Send data
            message = str(count) + ' TEST_MSG:1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
            print >>sys.stderr, 'Sending "%s"' % message
            body_len = len(message) 
            
            header = bytearray(4)
            header[0] = PROTOVERSION
            header[1] = OPCODE               
            header[2] = (body_len & 0xff)
            header[3] = ((body_len >> 8) & 0xff)        
            
            print >>sys.stderr, "Msg body length = " + str(body_len)
            
            sock.sendall(header)
            sock.send(message)
        
            # Look for the response
            # expected response is 3 bytes: "OK/0"            
            data = sock.recv(3)
                
            print >>sys.stderr, 'Received "%s"' % data
                
        finally:
            sock.close()
        

