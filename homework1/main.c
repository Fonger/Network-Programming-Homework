//
//  main.c
//  homework1
//
//  Created by 傑瑞 on 2015/10/26.
//  Copyright © 2015年 傑瑞. All rights reserved.
//

#include <stdio.h>
#include <stdarg.h>
#include "../lib/unp.h"

#define MAX_BUFF 100000
#define MAX_LINE 40000
#define MAX_CMDS 10000
#define MAX_ARGS 20
#define ERR_CMD_NOT_FOUND -5
#define TRUE 1
#define FALSE 0
#define MAX_USER 30

#define F_CONNECTING 0
#define F_READING 1
#define F_WRITING 2
#define F_DONE 3

typedef struct {
    int sockfd;
    int status;
} Client;

int new_client_fd(char *hostname, ushort port);

int main() {
    printf("Hello, World!\n");
    //chdir("/Users/jerry/Downloads/ras");

    fd_set rfds; /* readable file descriptor set */
    fd_set wfds; /* writable file descriptor set */
    fd_set rfds_a; /* active read file descriptor set */
    fd_set wfds_a; /* active write file descriptor set */
    
    //int nfds = 4;
    int nfds = 9;
    
    FD_ZERO(&rfds_a);
    FD_ZERO(&wfds_a);
    
    Client clients[5];
    
    for (int i = 0; i < 5; i++) {
        int clientfd = new_client_fd("nplinux3.cs.nctu.edu.tw", 9877);
        clients[i].sockfd = clientfd;
        clients[i].status = F_CONNECTING;
        FD_SET(clientfd, &rfds_a);
        FD_SET(clientfd, &wfds_a);
    }
    
    for (;;) {
        /* restore previous selected fds */
        memcpy(&rfds, &rfds_a, sizeof(rfds));
        memcpy(&wfds, &wfds_a, sizeof(wfds));
        
        Select(nfds, &rfds, &wfds, NULL, NULL);
        
        for (int i = 0; i < sizeof(clients); i++) {
            Client *c = &clients[i];
            if (c->status == F_CONNECTING &&
                (FD_ISSET(c->sockfd, &rfds) || FD_ISSET(c->sockfd, &wfds))) {


                int error;
                socklen_t clilen = sizeof(struct sockaddr_in);
                if (getsockopt(c->sockfd, SOL_SOCKET, SO_ERROR, &error, &clilen) < 0 ||
                    error != 0) {
                    fprintf(stderr, "non-blocking connect failed\n");
                    exit(1);
                }
                c->status = F_READING;
                FD_CLR(c->sockfd, &wfds);
            } else if (c->status == F_READING && FD_ISSET(c->sockfd, &rfds) ) {
                char buf[1024];
                Read(c->sockfd, buf, sizeof(buf));
                if (i == 0) printf("===%d===\n%s", i, buf);
                FD_CLR(c->sockfd, &rfds);
                FD_SET(c->sockfd, &wfds);
                c->status = F_WRITING;
            } else if (c->status == F_WRITING && FD_ISSET(c->sockfd, &wfds)) {
                if ( i == 0) {
                    printf("printenv path\n");
                    Write(c->sockfd, "printenv path\n", 14);
                }
                c->status = F_DONE;
                FD_CLR(c->sockfd, &wfds);
                FD_CLR(c->sockfd, &rfds);
            }
        }
    }
    return 0;
}


int new_client_fd(char *hostname, ushort port) {
    struct sockaddr_in servaddr;
    
    int clientfd = Socket(AF_INET, SOCK_STREAM, 0);
    
    struct hostent *he = gethostbyname(hostname);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr = *((struct in_addr *)he->h_addr);
    servaddr.sin_port = htons(port);

    // Set non-blocking
    int flags = fcntl(clientfd, F_GETFL, 0);
    fcntl(clientfd, F_SETFL, flags | O_NONBLOCK);

    if(connect(clientfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1) {
        if (errno != EINPROGRESS)
            err_sys("Connect failed");
    }
    return clientfd;
}

