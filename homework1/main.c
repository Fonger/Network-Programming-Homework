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


#define BUF_SIZE 100000
#define USERID_SIZE 50

#define TRUE 1
#define FALSE 0

typedef struct __attribute__((__packed__)) {
    unsigned char  VN;
    unsigned char  CD;
    unsigned short DST_PORT;
    unsigned int   DST_IP;
    unsigned char  userid[0];
} ClientInfo;

#define SOCK_GRANTED  90
#define SOCK_REJECTED 91

int new_socket();

char buffer[BUF_SIZE];

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

            ClientInfo *pclientInfo = malloc(sizeof(ClientInfo) + USERID_SIZE);

            if (Read(connfd, pclientInfo, sizeof(ClientInfo) + USERID_SIZE) < sizeof(ClientInfo))
                goto fail;
            
            if (pclientInfo->VN != 4) {
                printf("Not SOCK version 4\n");
                goto fail;
            }

            printf("version: %d\n", pclientInfo->VN);
            printf("cd: %d\n", pclientInfo->CD);
            printf("port: %d\n", ntohs(pclientInfo->DST_PORT));
            printf("ip: %x\n", ntohl(pclientInfo->DST_IP));
            
            if (pclientInfo->CD == 1) {
                // Connect mode

                // grant access
                pclientInfo->VN = 0;
                pclientInfo->CD = SOCK_GRANTED;
                Writen(connfd, pclientInfo, sizeof(ClientInfo));
                
                int rsockfd = Socket(AF_INET, SOCK_STREAM, 0);
                
                bzero(&servaddr, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_addr.s_addr = pclientInfo->DST_IP;
                servaddr.sin_port = pclientInfo->DST_PORT;
                
                Connect(rsockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
                
                fd_set rfds;
                fd_set rfds_a;
                
                FD_ZERO(&rfds_a);
                FD_SET(rsockfd, &rfds_a);
                FD_SET(connfd, &rfds_a);

                int nfds = 6;
                
                struct timeval timeout;
                timeout.tv_sec = 120;
                timeout.tv_usec = 0;
                
                while (TRUE) {
                    rfds = rfds_a;
                    Select(nfds, &rfds, NULL, NULL, &timeout);
                    if (FD_ISSET(rsockfd, &rfds)) {
                        ssize_t n = read(rsockfd, buffer, BUF_SIZE);
                        if (n > 0)
                            Writen(connfd, buffer, n);
                        else
                            FD_CLR(rsockfd, &rfds_a);
                    } else if (FD_ISSET(connfd, &rfds)) {
                        ssize_t n = read(connfd, buffer, BUF_SIZE);
                        if (n > 0)
                            Writen(rsockfd, buffer, n);
                        else
                            FD_CLR(connfd, &rfds_a);
                    } else break;
                }
                Close(rsockfd);
            } else if (pclientInfo->CD == 2) {
                // Bind mode
                
                int rlistenfd = Socket(AF_INET, SOCK_STREAM, 0);
                
                bzero(&servaddr, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
                servaddr.sin_port = htons(INADDR_ANY);

                Bind(rlistenfd, (SA *)&servaddr, sizeof(servaddr));
                socklen_t socklen = sizeof(servaddr);
                
                getsockname(rlistenfd, (SA *)&servaddr, &socklen);
                
                Listen(rlistenfd, LISTENQ);
                
                pclientInfo->VN = 0;
                pclientInfo->CD = SOCK_GRANTED;
                pclientInfo->DST_IP = htonl(INADDR_ANY);
                pclientInfo->DST_PORT = servaddr.sin_port;
                
                // First return our server ip (0 will be treated as proxy server) & port
                Writen(connfd, pclientInfo, sizeof(ClientInfo));
                
                // Then accept
                int rsockfd = Accept(rlistenfd, (SA *)&cliaddr, &clilen);
                
                Close(rlistenfd);
                
                // Check if client address equals to destination ip
                if (cliaddr.sin_addr.s_addr == pclientInfo->DST_IP) {
                    Writen(connfd, pclientInfo, sizeof(ClientInfo));
                    
                    fd_set rfds;
                    fd_set rfds_a;
                    
                    FD_ZERO(&rfds_a);
                    FD_SET(rsockfd, &rfds_a);
                    FD_SET(connfd, &rfds_a);
                    
                    int nfds = 7;
                    
                    struct timeval timeout;
                    timeout.tv_sec = 120;
                    timeout.tv_usec = 0;
                    
                    while (TRUE) {
                        rfds = rfds_a;
                        Select(nfds, &rfds, NULL, NULL, &timeout);
                        if (FD_ISSET(rsockfd, &rfds)) {
                            ssize_t n = read(rsockfd, buffer, BUF_SIZE);
                            if (n > 0)
                                Writen(connfd, buffer, n);
                            else
                                FD_CLR(rsockfd, &rfds_a);
                        } else if (FD_ISSET(connfd, &rfds)) {
                            ssize_t n = read(connfd, buffer, BUF_SIZE);
                            if (n > 0)
                                Writen(rsockfd, buffer, n);
                            else
                                FD_CLR(connfd, &rfds_a);
                        } else break;
                    }
                } else {
                    pclientInfo->CD = SOCK_REJECTED;
                    Write(connfd, pclientInfo, sizeof(ClientInfo));
                }
                Close(rsockfd);
            }
            printf("conn lost\n");
            fflush(stdout);
        fail:
            Close(connfd);
            free(pclientInfo);
            exit(0);
        }
    }
    return 0;
}
