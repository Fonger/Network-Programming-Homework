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

#define MAX_BUFF 5000

#define TRUE 1
#define FALSE 0

int main() {
    printf("Hello, World!\n");
    
    //chdir("/Users/jerry/Downloads/ras");
    
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

            ssize_t     n = 0;
            char        buf[MAX_BUFF];
            int         pos;
            
            pos = 0;
            do {
                n = Read(connfd, &buf[pos], MAX_BUFF - pos);
                pos += n;
            } while (buf[pos - 1] != '\n');
            buf[pos] = '\0';
            
            
            char *url;
            char *method;
            method = strtok(buf, " ");
            url = strtok(NULL, " ");

            
            //GET /test?qq=123 HTTP/1.1
            
            char *route;
            route = strtok(url, "?");
            
            char *cgi = route + 1;
            
            char *querystring;
            querystring = strtok(NULL, "");

            printf("url: %s\n", url);
            printf("method: %s\n", method);
            printf("route: %s\n", route);
            printf("cgi: %s\n", cgi);
            printf("querystring: %s\n", querystring);

            Dup2(connfd, STDOUT_FILENO);
            setenv("REQUEST_METHOD", method, TRUE);
            setenv("QUERY_STRING", querystring, TRUE);
            Close(connfd);
            
            exit(0);
        }
    }
    return 0;
}

