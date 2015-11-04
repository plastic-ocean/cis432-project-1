# CIS 432 Into to Networks
# Programming Project 1
# Fall 2015
#
# Benjamin Barnes
# H. Keith Hamm


CC=g++

CFLAGS=-Wall -W -g -Werror -std=c++11


all: client server

client: client.cpp raw.cpp
	$(CC) client.cpp raw.cpp $(CFLAGS) -o client

server: server.cpp
	$(CC) server.cpp $(CFLAGS) -o server

clean:
	rm -f client server *.o

