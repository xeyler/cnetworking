#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

// Can I use cmocka to mock BSD socket functions??
// TODO: unit test every code branch
// TODO: unit test multiple iterations of the while loop, when send() reports
// fewer bytes written than the message length

int main(char *argv, int argc) {
    int sockfd, newfd, status;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    const int TCP_BACKLOG = 128;
    const int YES = 1;
    char message[] = "HELLO, WORLD!\n";
    int bytessent = 0;

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

    if (listen(sockfd, TCP_BACKLOG)) {
        perror("listen");
        return 4;
    }

    addr_size = sizeof client_addr;
    if ((newfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size)) == -1) {
        perror("accept");
        close(sockfd);
        return 5;
    }

    while (bytessent < sizeof message) {
        if ((status = send(newfd, *(&message + bytessent), sizeof message - bytessent, 0)) == -1) {
            perror("send");
            close(sockfd);
            return 6;
        }
        bytessent += status;
    }

    shutdown(newfd, SHUT_RDWR);
    close(newfd);

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}