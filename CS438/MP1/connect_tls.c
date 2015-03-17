/*
 * ADAPTED FROM BY FRED DOUGLAS:
 *  SSL client demonstration program
 *
 *  Copyright (C) 2006-2013, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if !defined(POLARSSL_CONFIG_FILE)
#include "polarssl/config.h"
#else
#include POLARSSL_CONFIG_FILE
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "polarssl/net.h"
#include "polarssl/debug.h"
#include "polarssl/ssl.h"
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/error.h"
#include "polarssl/certs.h"


#include "connect_tls.h"

#define SERVER_PORT 8080

#ifndef POLARSSL_CERTS_C
#error PolarSSL doesnt have certificates...
#endif

x509_crt ourCert;
x509_crt rootCerts;
pk_context serverPrivateKey;

entropy_context entropy;
ctr_drbg_context ctr_drbg;

void stderrFromPolarCode(const char* info, int code)
{
	char errbuf[500];
	polarssl_strerror(code, errbuf, 500);
	fprintf(stderr, "====================\n%s\nError code -0x%x:\n%s\n====================\n",
		   info, -code, errbuf);
}

int clientInitTLS()
{
	int ret, len = -1;

	x509_crt_init(&ourCert);
	x509_crt_init(&rootCerts);
	entropy_init(&entropy);
	const char* extraData = "ssl_cs438";
	if((ret = ctr_drbg_init(&ctr_drbg, entropy_func, &entropy,
	                        (const unsigned char*) extraData,
	                        strlen(extraData))) != 0)
	{
		stderrFromPolarCode("PolarSSL failed to initialize PRNG: ctr_drbg_init returned:", ret);
		return ret;
	}

	const char* certPath = "the_servers_cert.crt";
	if(access(certPath, R_OK) != -1)
		ret = x509_crt_parse_file(&ourCert, certPath);
	else
	{
		fprintf(stderr, "The certificate file is corrupted or missing from %s!\n", certPath);
		return -1;
	}
	if(ret < 0)
	{
		stderrFromPolarCode("PolarSSL failed to parse the certificate: x509_crt_parse returned:", ret);
		return ret;
	}

	if(0!=x509_crt_parse_path(&rootCerts, "/usr/share/ca-certificates/mozilla"))
		fprintf(stderr, "Error loading root certificates.\n");

	return 0;
}

int serverInitTLS()
{
	int ret;
	x509_crt_init(&ourCert);
	entropy_init(&entropy);
	const char* extraData = "ssl_cs438";
	if((ret = ctr_drbg_init(&ctr_drbg, entropy_func, &entropy,
	                        (const unsigned char*) extraData,
	                        strlen(extraData))) != 0)
	{
		stderrFromPolarCode("PolarSSL failed to initialize PRNG: ctr_drbg_init returned:", ret);
		return ret;
	}

	const char* certPath = "the_servers_cert.crt";
	if(access(certPath, R_OK) != -1)
		ret = x509_crt_parse_file(&ourCert, certPath);
	else
	{
		fprintf(stderr, "The certificate file is corrupted or missing from %s!\n", certPath);
		return -1;
	}
	if(ret < 0)
	{
		stderrFromPolarCode("PolarSSL failed to parse the certificate: x509_crt_parse returned:", ret);
		return ret;
	}
	
	const char* keyPath = "the_servers_key.key";
	if(access(keyPath, R_OK) != -1)
		ret = pk_parse_keyfile(&serverPrivateKey, keyPath, 0);
	else
	{
		fprintf(stderr, "The key file is corrupted or missing from %s!\n", keyPath);
		return -1;
	}
	if(ret < 0)
	{
		stderrFromPolarCode("PolarSSL failed to parse the key: pk_parse_keyfile returned:", ret);
		return ret;
	}
}

//NOTE: serverName can be a domain name or IP address
TLS_Session* connectTLS(const char* serverName, unsigned short int serverPort)
{
	TLS_Session* newSession = (TLS_Session*)malloc(sizeof(TLS_Session));

	int ret;
	if((ret = net_connect(&(newSession->socket), serverName, serverPort)) != 0)
	{
		fprintf(stderr, "Failed to connect to %s:%d!\n", serverName, serverPort);
		stderrFromPolarCode("net_connect returned:", ret);
		free(newSession);
		return 0;
	}

	newSession->ssl = (ssl_context*)malloc(sizeof(ssl_context));
	memset(newSession->ssl, 0, sizeof(ssl_context));

	//2. Setup stuff
	if((ret = ssl_init(newSession->ssl)) != 0)
	{
		stderrFromPolarCode("PolarSSL failed to initialize: ssl_init returned:", ret);
		net_close(newSession->socket);
		free(newSession->ssl);
		free(newSession);
		return 0;
	}

	ssl_set_endpoint(newSession->ssl, SSL_IS_CLIENT);
	ssl_set_authmode(newSession->ssl, SSL_VERIFY_REQUIRED);

	//sort of HACK: pick either ourCert or rootCerts based on whether it's an IP address or domain name
	struct sockaddr_in dummy;
	if(inet_pton(AF_INET, serverName, &(dummy.sin_addr)))
		ssl_set_ca_chain(newSession->ssl, &ourCert, NULL, serverName);
	else
		ssl_set_ca_chain(newSession->ssl, &rootCerts, NULL, serverName);
	ssl_set_rng(newSession->ssl, ctr_drbg_random, &ctr_drbg);
	ssl_set_bio(newSession->ssl, net_recv, &(newSession->socket), net_send, &(newSession->socket));

	//4. Handshake
	while((ret = ssl_handshake(newSession->ssl)) != 0)
		if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE)
		{
			stderrFromPolarCode("PolarSSL's handshake with server failed: ssl_handshake returned:", ret);
			net_close(newSession->socket);
			ssl_free(newSession->ssl);
			free(newSession->ssl);
			free(newSession);
			return 0;
		}

	//5. Verify the server certificate
	if((ret = ssl_get_verify_result(newSession->ssl)) != 0)
	{
		if((ret & BADCERT_EXPIRED) != 0)
			fprintf(stderr, "Certificate presented by server has expired.\n");
		if((ret & BADCERT_REVOKED) != 0)
			fprintf(stderr, "Certificate presented by server has been revoked.\n");
		if((ret & BADCERT_CN_MISMATCH) != 0)
			fprintf(stderr,
			        "Common Name in certificate presented by server does not match (expected CN=%s).\n",
			        serverName);
		if((ret & BADCERT_NOT_TRUSTED) != 0)
			fprintf(stderr, "Certificate presented by server not in our trusted list.\n");
		if((ret & (BADCERT_NOT_TRUSTED | BADCERT_CN_MISMATCH | BADCERT_REVOKED | BADCERT_EXPIRED)) == 0)
			fprintf(stderr, "An unknown error occurred during certificate verification.\n");

		net_close(newSession->socket);
		ssl_free(newSession->ssl);
		free(newSession->ssl);
		free(newSession);
		return 0;
	}
	return newSession;
}

int listenTCP(const char* bindIP, unsigned short int bindPort)
{
	int theSocket, ret;
	if((ret = net_bind(&theSocket, bindIP, bindPort)) != 0)
	{
		stderrFromPolarCode("bind() and listen() failure: net_bind returned:", ret);
		return ret; //polarssl errors are all negative; no worries
	}
	return theSocket;
}

int acceptTCP(int listeningSocket)
{
	int ret, acceptedSocket;
	if((ret = net_accept(listeningSocket, &acceptedSocket, NULL)) != 0)
	{
		stderrFromPolarCode("accept() failed! net_accept returned:", ret);
		return -1;
	}
	return acceptedSocket;
}

TLS_Session* acceptTLS(int clientSocket)
{
	int ret;

	if((ret = ctr_drbg_reseed(&ctr_drbg, "cs438child", strlen("cs438child"))) != 0)
	{
		stderrFromPolarCode("PRNG reseed failed: ctr_drbg_reseed returned:", ret);
		return 0;
	}

	TLS_Session* newSession = (TLS_Session*)malloc(sizeof(TLS_Session));
	newSession->ssl = (ssl_context*)malloc(sizeof(ssl_context));
	newSession->socket = clientSocket;

	if((ret = ssl_init(newSession->ssl)) != 0)
	{
		stderrFromPolarCode("PolarSSL failed to initialize: ssl_init returned:", ret);
		free(newSession->ssl);
		free(newSession);
		return 0;
	}

	ssl_set_endpoint(newSession->ssl, SSL_IS_SERVER);
	ssl_set_authmode(newSession->ssl, SSL_VERIFY_NONE);
	ssl_set_rng(newSession->ssl, ctr_drbg_random, &ctr_drbg);
	//ssl_set_dbg(newSession->ssl, my_debug, stderr);
	ssl_set_bio(newSession->ssl, net_recv, &(newSession->socket),
	            net_send, &(newSession->socket));
	ssl_set_ca_chain(newSession->ssl, ourCert.next, NULL, NULL);
	if((ret = ssl_set_own_cert(newSession->ssl, &ourCert, &serverPrivateKey)) != 0)
	{
		stderrFromPolarCode("Failed to set key and cert: ssl_set_own_cert returned:", ret);
		ssl_free(newSession->ssl);
		free(newSession->ssl);
		free(newSession);
		return 0;
	}
	while((ret = ssl_handshake(newSession->ssl)) != 0)
	{
		if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE)
		{
			stderrFromPolarCode("PolarSSL's handshake with server failed: ssl_handshake returned:", ret);
			ssl_free(newSession->ssl);
			free(newSession->ssl);
			free(newSession);
			return 0;
		}
	}
	
	return newSession;
}

int sendTLS(TLS_Session* session, const char* buf, unsigned int len)
{
	int ret;
	while((ret = ssl_write(session->ssl, buf, len)) <= 0)
		if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE)
		{
			stderrFromPolarCode("PolarSSL send() failure: ssl_write returned:", ret);
			return ret;
		}
	return ret;
}

int recvTLS(TLS_Session* session, char* buf, unsigned int len)
{
	int ret;
	memset(buf, 0, len);
	while((ret = ssl_read(session->ssl, buf, len)) < 0)
		if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE)
		{
			if(ret == POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY)
				return ret;
			stderrFromPolarCode("PolarSSL recv() failure: ssl_read returned:", ret);
			return ret;
		}
	return ret;
}

void shutdownTLS(TLS_Session* session)
{
	if(session->ssl==NULL)
	{
		if(session->socket != -1)
			net_close(session->socket);
		free(session);
		return;
	}

	ssl_close_notify(session->ssl);

	if(session->socket != -1)
		net_close(session->socket);
	ssl_free(session->ssl);
	memset(session->ssl, 0, sizeof(ssl_context));
	free(session->ssl);
	free(session);
}

//------------------------------------------------------------------
//ctr_drbg_free(&ctr_drbg)... not in the standard ubuntu package
//Implementation that should never be optimized out by the compiler
static void cs438_polarssl_zeroize( void *v, size_t n )
{volatile unsigned char *p = v; while( n-- ) *p++ = 0;}
void cs438_aes_free( aes_context *ctx )
{
	if( ctx == NULL )
		return;
	cs438_polarssl_zeroize( ctx, sizeof( aes_context ) );
}
void cs438_ctr_drbg_free( ctr_drbg_context *ctx )
{
	if( ctx == NULL )
		return;
	cs438_aes_free( &ctx->aes_ctx );
	cs438_polarssl_zeroize( ctx, sizeof( ctr_drbg_context ) );
}
//------------------------------------------------------------------
void clientUninitTLS()
{
	x509_crt_free(&ourCert);
	x509_crt_free(&rootCerts);
	entropy_free(&entropy);
	cs438_ctr_drbg_free(&ctr_drbg);
}

void serverUninitTLS()
{
	x509_crt_free(&ourCert);
	pk_free(&serverPrivateKey);
	entropy_free(&entropy);
	cs438_ctr_drbg_free(&ctr_drbg);
}
