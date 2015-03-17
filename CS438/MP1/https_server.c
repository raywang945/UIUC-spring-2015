#include <stdlib.h>
#include <unistd.h>

#include "connect_tls.h"

#define BACKLOG       10	 // how many pending connections queue will hold
#define MAXDATASIZE   50000 // max number of bytes we can get at once
#define DATALEN       2048

int main(int argc, char *argv[])
{
	int sockfd, new_fd, numbytes;  // listen on sock_fd, new connection on new_fd
    char buf[MAXDATASIZE];
    char *tok = NULL, *tok2 = NULL;

    if (serverInitTLS() == -1) {
        perror("serverInitTLS");
        exit(1);
    }

    if ((sockfd = listenTCP(NULL, atoi(argv[1]))) == -1) {
        perror("listenTCP");
        exit(1);
    }

	printf("server: waiting for connections...\n");

    TLS_Session *session;

	while(1) {  // main accept() loop
        new_fd = acceptTCP(sockfd);
        if (new_fd == -1) {
            perror("acceptTCP");
            continue;
        }

		if (!fork()) { // this is the child process
            close(sockfd);
            session = acceptTLS(new_fd);

            numbytes = recvTLS(session, buf, MAXDATASIZE - 1);
            buf[numbytes] = '\0';

            tok = strstr(buf, "GET /");
            if (tok != buf) {
                memset(buf, '\0', MAXDATASIZE);
                sprintf(buf, "HTTP/1.0 400 Bad Request\r\n\r\n");
                if ((numbytes = sendTLS(session, buf, strlen(buf))) == -1) {
                    perror("send");
                    exit(1);
                }
                exit(1);
            }

            tok += 5;
            tok2 = strstr(tok, "HTTP/") - 1;

            if (strncmp(tok2, " HTTP/", 6) != 0) {
                memset(buf, '\0', MAXDATASIZE);
                sprintf(buf, "HTTP/1.0 400 Bad Request\r\n\r\n");
                if ((numbytes = sendTLS(session, buf, strlen(buf))) == -1) {
                    perror("send");
                    exit(1);
                }
                exit(1);
            }

            char file_path[tok2 - tok + 1];
            sprintf(file_path, "%.*s", (int)(tok2 - tok), tok);
            file_path[tok2 - tok] = '\0';

            if (strncmp(tok + strlen(tok) - 4, "\r\n\r\n", 4) != 0) {
                memset(buf, '\0', MAXDATASIZE);
                sprintf(buf, "HTTP/1.0 400 Bad Request\r\n\r\n");
                if ((numbytes = sendTLS(session, buf, strlen(buf))) == -1) {
                    perror("send");
                    exit(1);
                }
                exit(1);
            }

            FILE *fd;
            fd = fopen(file_path, "rb");

            memset(buf, '\0', MAXDATASIZE);
            if (!fd) {
                sprintf(buf, "HTTP/1.0 404 Not Found\r\n\r\n");
                if ((numbytes = sendTLS(session, buf, strlen(buf))) == -1) {
                    perror("send");
                    exit(1);
                }
                perror("fopen");
                exit(1);
            }

            sprintf(buf, "HTTP/1.0 200 OK\r\n\r\n");
            if ((numbytes = fread(buf + strlen(buf), sizeof(char), DATALEN - strlen(buf), fd)) != 0) {
                if ((numbytes = sendTLS(session, buf, numbytes + strlen("HTTP/1.0 200 OK\r\n\r\n"))) == -1) {
                    perror("send");
                    exit(1);
                }
            }
            memset(buf, '\0', MAXDATASIZE);
            while ((numbytes = fread(buf, sizeof(char), DATALEN, fd)) != 0) {
                if ((numbytes = sendTLS(session, buf, numbytes)) == -1) {
                    perror("send");
                    exit(1);
                }
                memset(buf, '\0', DATALEN);
            }

            fclose(fd);

			close(new_fd);
            shutdownTLS(session);
            serverUninitTLS();
			exit(0);
		}
        close(new_fd);
	}

	return 0;
}
