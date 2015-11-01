CC=g++

CFLAGS=-Wall -W -g -Werror -std=c++11


all: client server client_bad server_bad

client: client.cpp raw.cpp
	$(CC) client.cpp raw.cpp $(CFLAGS) -o client

client_bad: client_bad.cpp raw.cpp
	$(CC) client_bad.cpp raw.cpp $(CFLAGS) -o client_bad

server: server.cpp
	$(CC) server.cpp $(CFLAGS) -o server

server_bad: server_bad.cpp
	$(CC) server_bad.cpp $(CFLAGS) -o server_bad

clean:
	rm -f client server client_bad server_bad *.o

