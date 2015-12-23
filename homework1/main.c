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
#define PARTIAL_SIZE 50
#define TRUE 1
#define FALSE 0

typedef struct __attribute__((__packed__)) {
    unsigned char  VN;
    unsigned char  CD;
    unsigned short DST_PORT;
    unsigned int   DST_IP;
    unsigned char  userid[0];
} ClientInfo;

#define SOCK_CONNECT  1
#define SOCK_BIND     2
#define SOCK_GRANTED  90
#define SOCK_REJECTED 91

void proxy_pass(int csock, int rsock);
ssize_t ReadPrint(int sockfd, char *buffer, ssize_t size);

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

            printf("======SOCK ESTABLISHED======\n");
            printf("VN: %d CD: %d\n", pclientInfo->VN, pclientInfo->CD);
            
            char *dst_ip = (char*)&pclientInfo->DST_IP;
            printf("DST_IP: %u.%u.%u.%u, ", dst_ip[0] & 0xFF, dst_ip[1] & 0xFF, dst_ip[2] & 0xFF, dst_ip[3] & 0xFF);
            printf("DST_PORT: %d\n", ntohs(pclientInfo->DST_PORT));
            
            char *src_ip = (char *)&cliaddr.sin_addr.s_addr;
            printf("SRC_IP: %u.%u.%u.%u, ", src_ip[0] & 0xFF, src_ip[1] & 0xFF, src_ip[2] & 0xFF, src_ip[3] & 0xFF);
            printf("SRC_PORT: %d\n", ntohs(cliaddr.sin_port));


            
            if (pclientInfo->CD == SOCK_CONNECT) {
                // Connect mode

                printf("SOCKS_CONNECT GRANTED\n");
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
                
                proxy_pass(connfd, rsockfd);
                
                Close(rsockfd);
            } else if (pclientInfo->CD == SOCK_BIND) {
                // Bind mode
                
                printf("SOCKS_BIND GRANTED\n");
                int rlistenfd = Socket(AF_INET, SOCK_STREAM, 0);
                
                bzero(&servaddr, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
                servaddr.sin_port = htons(INADDR_ANY);

                Bind(rlistenfd, (SA *)&servaddr, sizeof(servaddr));
                socklen_t socklen = sizeof(servaddr);
                
                getsockname(rlistenfd, (SA *)&servaddr, &socklen);
                
                Listen(rlistenfd, LISTENQ);
                
                in_addr_t remote_addr = pclientInfo->DST_IP;
                
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
                if (cliaddr.sin_addr.s_addr == remote_addr) {
                    Writen(connfd, pclientInfo, sizeof(ClientInfo));
                    proxy_pass(connfd, rsockfd);
                } else {
                    printf("SOCKS_BIND Client Failed\n");
                    pclientInfo->CD = SOCK_REJECTED;
                    Write(connfd, pclientInfo, sizeof(ClientInfo));
                }
                Close(rsockfd);
            }
        fail:
            Close(connfd);
            free(pclientInfo);
            fflush(stdout);
            exit(0);
        }
    }
    return 0;
}

void proxy_pass(int csock, int rsock) {
    fd_set rfds;
    fd_set rfds_a;
    
    FD_ZERO(&rfds_a);
    FD_SET(rsock, &rfds_a);
    FD_SET(csock, &rfds_a);
    
    int nfds = (csock > rsock ? csock : rsock) + 1;
    ssize_t n;
    
    struct timeval timeout;
    timeout.tv_sec = 120;
    timeout.tv_usec = 0;
    
    while (TRUE) {
        rfds = rfds_a;
        Select(nfds, &rfds, NULL, NULL, &timeout);
        if (FD_ISSET(rsock, &rfds)) {
            n = ReadPrint(rsock, buffer, BUF_SIZE);
            if (n > 0)
                Writen(csock, buffer, n);
            else
                break;
        } else if (FD_ISSET(csock, &rfds)) {
            n = ReadPrint(csock, buffer, BUF_SIZE);
            if (n > 0)
                Writen(rsock, buffer, n);
            else
                break;
        } else break;
    }
}

ssize_t ReadPrint(int sockfd, char *buffer, ssize_t size) {
    ssize_t n = read(sockfd, buffer, size);
    if (n <= 0)
        return n;

    printf("Proxying %lu bytes...", n);
    
    for (ssize_t i = 0; i < n && i < PARTIAL_SIZE; i++) {
        if (i % 10 == 0)
            printf("\n");
        printf("%02x ", buffer[i] & 0xFF);
    }

    if (n < PARTIAL_SIZE)
        printf("...\n");
    
    fflush(stdout);
    return n;
}
