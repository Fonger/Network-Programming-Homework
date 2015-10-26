//
//  main.c
//  homework1
//
//  Created by 傑瑞 on 2015/10/26.
//  Copyright © 2015年 傑瑞. All rights reserved.
//

#include <stdio.h>
#include "unp.h"

void showSymbol(int sockfd);
void receive_cmd(int sockfd);

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
    
    for (;;) {
        clilen = sizeof(cliaddr);
        connfd = Accept(listenfd, (SA *) &cliaddr, &clilen);

        //Close(listenfd);   // close listening socket
        receive_cmd(connfd);
        Close(connfd);       // parent closes connected socket
    }
    

    return 0;
}


void receive_cmd(int sockfd)
{
    char welcome1[] = "****************************************\n";
    char welcome2[] = "** Welcome to the information server. **\n";
    
    Writen(sockfd, welcome1, sizeof(welcome1));
    Writen(sockfd, welcome2, sizeof(welcome2));
    Writen(sockfd, welcome1, sizeof(welcome1));
    
    showSymbol(sockfd);
    ssize_t		n;
    char		buf[MAXLINE];
    setenv("PATH", "/bin:.", TRUE);


    //Close(sockfd);
    
again:
    while ( (n = read(sockfd, buf, MAXLINE)) > 0) {
        printf("Receive a command!\n");
        pid_t pid = Fork();
        if (pid > 0) { // parent
            int status = 0;
            while( (wait( &status ) == -1) && (errno == EINTR) );
        } else if (pid == 0) { // child

            Dup2(sockfd, STDOUT_FILENO);
            Dup2(sockfd, STDERR_FILENO);
            Close(sockfd);

            
            buf[n] = '\0';

            char *delim = " \n";
            
            int argc = 0;
            char *argv[255];
            
            char *p = strtok(buf, delim);

            if ( p == NULL) {
                exit(0);
            }

            argv[argc++] = p;
            while ((p = strtok(NULL, delim)) != NULL) {
                argv[argc++] = p;
            }

            argv[argc] = NULL;
            
            printf("argc = %d\n", argc);
            
            for (int i = 0; i < argc; i++) {
                printf("argv[%d] = %s\n", i, argv[i]);
            }

            execvp(argv[0], argv);
            
            printf("Unknown Command: [%s]\n", argv[0]);
            exit(-1);
        } else {
            err_sys("fork failed");
        }
    }
    if (n < 0 && errno == EINTR) {
        goto again;
    }
    else if (n < 0) {
        printf("n=%lu\n", n);
        err_sys("str_echo: read error");
    }
}

void showSymbol(int sockfd) {
    char *symbol = "% ";
    Writen(sockfd, symbol, 2);
}
