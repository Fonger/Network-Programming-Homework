//
//  main.c
//  homework1
//
//  Created by 傑瑞 on 2015/10/26.
//  Copyright © 2015年 傑瑞. All rights reserved.
//

#include <stdio.h>
#include "unp.h"
#define MAXARGS 250

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
    char        *argv[MAXARGS];
    char        *delim = " \n";

    setenv("PATH", "/Users/Fonger/ras/bin:/bin:.", TRUE);

again:
    while ( (n = read(sockfd, buf, MAXLINE)) > 0) {
        buf[n] = '\0';

        char *p = strtok(buf, delim);
        
        if ( p == NULL)
            continue;

        if (strcmp(p, "exit") == 0)
            return;
        
        int argc = 0;

        argv[argc++] = p;
        while ((p = strtok(NULL, delim)) != NULL) {
            argv[argc++] = p;
        }
        
        argv[argc] = NULL;
        
        for (int i = 0; i < argc; i++)
            printf("argv[%d] = %s\n", i, argv[i]);

        if (strcmp(argv[0], "printenv") == 0) {
            for (int i = 1; i < argc; i++)
                dprintf(sockfd, "%s=%s\n", argv[i], getenv(argv[i]));
            showSymbol(sockfd);
            continue;
        }

        if (strcmp(argv[0], "setenv") == 0) {
            if (argc == 3)
                setenv(argv[1], argv[2], TRUE);
            else
                dprintf(sockfd, "usage: setenv KEY VALIE\n");
            showSymbol(sockfd);
            continue;
        }
        
        pid_t child_pid = Fork();
        if (child_pid > 0) { // parent
            printf("Child spawn with pid: %d\n", child_pid);

            int status = 0;
            while( (wait( &status ) == -1) && (errno == EINTR) );

            if (WIFEXITED(status)) {
                char exit_code = WEXITSTATUS(status);
                printf("Child exit with code: %d\n", exit_code);
                showSymbol(sockfd);
            }

        } else if (child_pid == 0) { // child
            Dup2(sockfd, STDOUT_FILENO);
            Dup2(sockfd, STDERR_FILENO);
            Close(sockfd);

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
        err_sys("str_echo: read error");
    }
}

void showSymbol(int sockfd) {
    char *symbol = "% ";
    Writen(sockfd, symbol, 2);
}
