#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

// Waiting on children avoids the creation of zombie processes
void sigchld_handler(int s) {
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int main(char *argv, int argc) {
    int sockfd, newfd, status, bytestosend, bytessent;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    struct sigaction sa;
    socklen_t addr_size;
    const int BUFFER_SIZE = 10;
    const int TCP_BACKLOG = 128;
    const int YES = 1;
    char mbuf[BUFFER_SIZE];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (status = getaddrinfo(NULL, "22760", &hints, &servinfo)) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    for (p = servinfo; p; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket error");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &YES, sizeof YES)) {
            close(sockfd);
            freeaddrinfo(servinfo);
            perror("setsockopt");
            return 2;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen)) {
            close(sockfd);
            perror("bind");
            continue;
        }
        
        break;
    }

    freeaddrinfo(servinfo);

    if (!p) {
        fprintf(stderr, "Could not bind\n");
        return 3;
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 4;
    }

    if (listen(sockfd, TCP_BACKLOG)) {
        perror("listen");
        return 5;
    }

    printf("listening for connections...\n");

    while (1) {
        addr_size = sizeof client_addr;
        if ((newfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size)) == -1) {
            perror("accept");
            close(sockfd);
            continue;
        }

        if (!fork()) {
            close(sockfd);

            // Until the client terminates the connection, echo messages BUFFER_SIZE bytes at a time
            while ((status = recv(newfd, (void *)mbuf, BUFFER_SIZE, 0))) {
                if (status == -1) {
                    perror("recv");
                    shutdown(newfd, SHUT_RDWR);
                    close(newfd);
                    return 6;
                }

                bytestosend = status;
                bytessent = 0;

                while (bytestosend) {
                    if ((status = send(newfd, *(&mbuf + bytessent), bytestosend, 0)) == -1) {
                        perror("send");
                        shutdown(newfd, SHUT_RDWR);
                        close(newfd);
                        return 7;
                    }
                    bytestosend -= status;
                    bytessent += status;
                }
            }
            shutdown(newfd, SHUT_RDWR);
            close(newfd);
            return 0;
        }
        close(newfd);
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}