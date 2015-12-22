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
    unsigned char  userid[0];
} ClientInfo;

#define SOCK_GRANTED 70

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
            printf("port: %d\n", htons(clientInfo.DST_PORT));
            printf("ip: %x\n", htonl(clientInfo.DST_IP));
            
            if (clientInfo.CD == 1) {
                printf("Connect mode=====\n");
                clientInfo.CD = SOCK_GRANTED;
                Writen(connfd, &clientInfo, sizeof(ClientInfo));
                
                int rsockfd = Socket(AF_INET, SOCK_STREAM, 0);
                
                bzero(&servaddr, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_addr.s_addr = clientInfo.DST_IP;
                servaddr.sin_port = clientInfo.DST_PORT;
                
                
                fd_set rfds;
                fd_set wfds;
                fd_set fds;

                bzero(&fds, sizeof(rfds));
                
                FD_SET(rsockfd, &fds);

                
                int nfds = 7;
                
                while (TRUE) {
                    rfds = fds;
                    wfds = fds;
                    Select(nfds, &rfds, &wfds, NULL, NULL);
                    if (FD_ISSET(rsockfd, &rfds)) {

                    } else if (FD_ISSET(rsockfd, &wfds)) {
                        
                    }
                }
                
            }

        fail:
            Close(connfd);
        }
    }
    return 0;
}
