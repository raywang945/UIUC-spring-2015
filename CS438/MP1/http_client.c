#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>

#include <arpa/inet.h>

#include "connect_tls.h"

#define MAXDATASIZE      50000 // max number of bytes we can get at once
#define PATHLEN          1000
#define HEADERBUFSIZE    5000
#define OUTPUTFILENAME   "tmp"
#define FINALOUTPUT      "output"

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes, i;
    char buf[MAXDATASIZE], path[PATHLEN], header_buf[HEADERBUFSIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv, port_length, is_http;
	char s[INET6_ADDRSTRLEN];
    char *tok = NULL;

    TLS_Session *session;

	if (argc != 2) {
	    fprintf(stderr,"usage: http_client <url file path>\n");
	    exit(1);
	}

    // parse protocol
    tok = strtok(argv[1], ":");
    char protocol[strlen(tok) + 1];
    strcpy(protocol, tok);
    protocol[strlen(tok)] = '\0';
    for (i = 0; i < strlen(protocol); i ++) {
        protocol[i] = toupper(protocol[i]);
    }
    // get is_http
    is_http = strcmp(protocol, "HTTP") == 0 ? 1 : 0;
    // parse host
    tok = strtok(NULL, "/");
    char host[strlen(tok) + 1];
    strcpy(host, tok);
    host[strlen(tok)] = '\0';
    // parse
    tok = strtok(NULL, "\0");
    memset(path, '\0', sizeof(path));
    if (tok == NULL) {
        sprintf(path, "/");
    } else {
        sprintf(path, "/%s", tok);
    }

    tok = strtok(host, ":");
    tok = strtok(NULL, "\0");
    if (tok == NULL) {
        if (is_http) {
            port_length = 3;
        } else {
            port_length = 4;
        }
    } else {
        port_length = strlen(tok) + 1;
    }
    char port[port_length];
    if (tok == NULL) {
        if (is_http) {
            strcpy(port, "80");
            port[2] = '\0';
        } else {
            strcpy(port, "443");
            port[3] = '\0';
        }
    } else {
        strcpy(port, tok);
        port[strlen(tok)] = '\0';
    }

    if (is_http) {
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }

        // loop through all the results and connect to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
            }

            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                perror("client: connect");
                continue;
            }

            break;
        }

        if (p == NULL) {
            fprintf(stderr, "client: failed to connect\n");
            return 2;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                s, sizeof s);
        printf("client: connecting to %s\n", s);

        freeaddrinfo(servinfo); // all done with this structure
    } else {
        sockfd = clientInitTLS();
        session = connectTLS(host, atoi(port));
    }

    memset(buf, '\0', sizeof buf);
    sprintf(buf, "GET %s HTTP/1.0\r\n", path);
    sprintf(buf + strlen(buf), "User-Agent: Wget/1.12 (linux-gnu)\r\n");
    sprintf(buf + strlen(buf), "Accept: */*\r\n");
    sprintf(buf + strlen(buf), "Host: %s:%s\r\n", host, port);
    sprintf(buf + strlen(buf), "Connection: Keep-Alive\r\n\r\n");

    if (is_http) {
        if ((numbytes = send(sockfd, buf, strlen(buf), 0)) == -1) {
            perror("send");
            exit(1);
        }
    } else {
        if ((numbytes = sendTLS(session, buf, strlen(buf))) == -1) {
            perror("sendTLS");
            exit(1);
        }
    }

    FILE *fd;
    fd = fopen(OUTPUTFILENAME, "wb");

    /*int is_header_processed = 0;*/
    /*memset(header_buf, '\0', HEADERBUFSIZE);*/
    while (1) {
        if (is_http) {
            if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }
        } else {
            if ((numbytes = recvTLS(session, buf, MAXDATASIZE-1)) == -1) {
                perror("recvTLS");
                exit(1);
            }
        }
        if (numbytes == 0) {
            break;
        }
        /*sprintf(header_buf, "%s", buf);*/
        /*if (tok == NULL) {*/
            /*puts("NULL");*/
        /*} else {*/
            /*puts("not NULL");*/
        /*}*/
        fwrite(buf, sizeof(char), numbytes, fd);
        /*if (!is_header_processed) {*/
            /*tok = strstr(buf, "\r\n\r\n") + 4;*/
            /*fwrite(tok, sizeof(char), numbytes - (tok - buf), fd);*/
            /*is_header_processed = 1;*/
        /*} else {*/
            /*fwrite(buf, sizeof(char), numbytes, fd);*/
        /*}*/
    }

    if (is_http) {
        close(sockfd);
    } else {
        shutdownTLS(session);
        clientUninitTLS();
    }

    fclose(fd);

    fd = fopen(OUTPUTFILENAME, "rb");
    FILE *fd_final;
    fd_final = fopen(FINALOUTPUT, "wb");
    int is_header_processed = 0;
    do {
        numbytes = fread(buf, sizeof(char), MAXDATASIZE, fd);
        if (!is_header_processed) {
            tok = strstr(buf, "\r\n\r\n") + 4;
            fwrite(tok, sizeof(char), numbytes - (tok - buf), fd_final);
            is_header_processed = 1;
        } else {
            fwrite(buf, sizeof(char), numbytes, fd_final);
        }
    } while (numbytes == MAXDATASIZE);
    fclose(fd_final);

    fclose(fd);

    remove(OUTPUTFILENAME);

	return 0;
}
