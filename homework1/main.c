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


#include   <sys/types.h>
#include   <sys/ipc.h>
#include   <sys/shm.h>


#define MAX_BUFF 100000
#define MAX_LINE 40000
#define MAX_CMDS 10000
#define MAX_ARGS 20
#define MAX_NAME 20
#define ERR_CMD_NOT_FOUND -5
#define TRUE 1
#define FALSE 0
#define MAX_USER 30
#define MAX_CHAT 2000

typedef struct __attribute__((__packed__)) {
    unsigned char  VN;
    unsigned char  CD;
    unsigned short DST_PORT;
    unsigned int   DST_IP;
    unsigned char  userid[4];
} ClientInfo;

#define SOCK_GRANTED 90

int new_socket();

char buffer[MAX_BUFF];

int main() {
    printf("Hello, World!\n");
    int listenfd, connfd;

    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    Bind(listenfd, (SA *) &servaddr, sizeof(servaddr));
    
    Listen(listenfd, LISTENQ);

    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        err_sys("Can't set SIGCHLD signal to SIG_IGN");
    }
    
    for (;;) {
        clilen = sizeof(cliaddr);
        connfd = Accept(listenfd, (SA *) &cliaddr, &clilen);
        
        pid_t child_pid = Fork();
        if (child_pid > 0) {
            Close(connfd);
        } else if (child_pid == 0) {
            /* child */
            Close(listenfd);

            ClientInfo clientInfo;
            if (Read(connfd, &clientInfo, sizeof(clientInfo)) < sizeof(clientInfo))
                goto fail;
            
            if (clientInfo.VN != 4) {
                printf("Not SOCK version 4\n");
                goto fail;
            }

            printf("version: %d\n", clientInfo.VN);
            printf("cd: %d\n", clientInfo.CD);
            printf("port: %d\n", ntohs(clientInfo.DST_PORT));
            printf("ip: %x\n", ntohl(clientInfo.DST_IP));
            
            if (clientInfo.CD == 1) {
                // Connect mode

                // grant access
                clientInfo.VN = 0;
                clientInfo.CD = SOCK_GRANTED;
                Writen(connfd, &clientInfo, sizeof(ClientInfo));
                
                int rsockfd = Socket(AF_INET, SOCK_STREAM, 0);
                
                bzero(&servaddr, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_addr.s_addr = clientInfo.DST_IP;
                servaddr.sin_port = clientInfo.DST_PORT;
                
                Connect(rsockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
                
                fd_set rfds;
                fd_set rfds_a;
                
                FD_ZERO(&rfds_a);
                FD_SET(rsockfd, &rfds_a);
                FD_SET(connfd, &rfds_a);

                int nfds = ( (connfd > rsockfd) ? connfd : rsockfd ) + 1;
                
                struct timeval timeout;
                timeout.tv_sec = 30;
                timeout.tv_usec = 0;
                
                while (TRUE) {
                    rfds = rfds_a;
                    Select(nfds, &rfds, NULL, NULL, &timeout);
                    if (FD_ISSET(rsockfd, &rfds)) {
                        ssize_t n = read(rsockfd, buffer, MAX_BUFF);
                        if (n > 0)
                            Writen(connfd, buffer, n);
                        else
                            FD_CLR(rsockfd, &rfds_a);
                    } else if (FD_ISSET(connfd, &rfds)) {
                        ssize_t n = read(connfd, buffer, MAX_BUFF);
                        if (n > 0)
                            Writen(rsockfd, buffer, n);
                        else
                            FD_CLR(connfd, &rfds_a);
                    } else break;
                }
                Close(rsockfd);
            } else if (clientInfo.CD == 2) {
                // Bind mode
                
            }
            printf("conn lost\n");
            fflush(stdout);
        fail:
            Close(connfd);
            exit(0);
        }
    }
    return 0;
}
