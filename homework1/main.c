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

void bad_request();
void home_page();

int main() {
    printf("Hello, World!\n");
    
    chdir("/Users/Fonger/Desktop/HW3 (2)/server_file");
    
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
            
            Dup2(connfd, STDOUT_FILENO);
            Close(connfd);

            char *url;
            char *method;
            method = strtok(buf, " ");
            
            if (*method == '\0')
                bad_request();
            
            url = strtok(NULL, " ");
            
            if (url == NULL || *url == '\0')
                bad_request();
            
            //GET /test?qq=123 HTTP/1.1
            
            char *route;
            route = strtok(url, "?");
            
            if (route == NULL || *route == '\0')
                home_page();
            
            char *cgi = route + 1;
            
            if (*cgi == '\0')
                home_page();
            
            char *querystring;
            querystring = strtok(NULL, "");
            
            setenv("REQUEST_METHOD", method, TRUE);

            if (querystring != NULL)
                setenv("QUERY_STRING", querystring, TRUE);

            printf("HTTP/1.1 200 OK\n");
            execl(cgi, cgi, NULL);
            printf("<h1>CGI not found</h1>");
        }
    }
    return 0;
}

void bad_request() {
    printf("HTTP/1.1 400 BAD REQUEST\nContent-Type: text/html\n\n<h1>Bad Request</h1>");
    exit(0);
}

void home_page() {
    printf("HTTP/1.1 200 OK\nContent-Type: text/html\n\n<h1>Home Page</h1>");
    exit(0);
}