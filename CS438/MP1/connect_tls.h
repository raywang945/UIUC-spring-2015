#ifndef __CS438_CONNECTTLS_INCLGUARD_
#define __CS438_CONNECTTLS_INCLGUARD_

#include "polarssl/ssl.h"

typedef struct TLS_Session
{
	ssl_context* ssl;
	int socket;
} TLS_Session;

//your HTTPS server should use this one
int serverInitTLS();
//you HTTPS client should use this one
int clientInitTLS();

TLS_Session* connectTLS(const char* serverName, unsigned short int serverPort);

//returns a socket file descriptor that you can pass to acceptTCP. 
//passing null for bindIP is equivalent to giving INADDR_ANY to regular bind().
int listenTCP(const char* bindIP, unsigned short int bindPort);

//NOTE: call acceptTCP() before fork()ing, and acceptTLS() after!
//acceptTCP returns a socket file descriptor that can be passed to acceptTLS.
int acceptTCP(int listeningSocket);
TLS_Session* acceptTLS(int clientSocket);

int sendTLS(TLS_Session* ssl, const char* buf, unsigned int len);
int recvTLS(TLS_Session* ssl, char* buf, unsigned int len);
//shutdownTLS does some free()ing of things that connectTLS() or
//acceptTLS() malloc()s, so don't forget it!
void shutdownTLS(TLS_Session* ssl);

//Your HTTPS server should use this one. If you use fork(), each child process
//should call serverUninitTLS() right before it exits.
void serverUninitTLS();
//you HTTPS client should use this one.
void clientUninitTLS();

#endif
