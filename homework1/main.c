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
void not_found();

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
            
            char *route = strtok(url, "?");
            
            if (route == NULL || *route == '\0')
                home_page();
            
            route++; // exclude the first '/' character
            
            if (*route == '\0')
                home_page();
            
            char *querystring;
            querystring = strtok(NULL, "");
            char *ext_start = strrchr(route, '.');

            if (ext_start == NULL || strncasecmp(ext_start, ".cgi\0", 5) != 0) {
                
                if (strncasecmp(ext_start, ".htm", 4) == 0) {
                    printf("HTTP/1.1 200 OK\n");
                    printf("Content-Type: text/html\n\n");
                    
                    FILE *file = fopen(route, "r");
                    if (file == NULL)
                        not_found();

                    int c;
                    while ((c = getc(file)) != EOF)
                        putchar(c);
                    fclose(file);
                }
            } else {

                setenv("REQUEST_METHOD", method, TRUE);
                
                if (querystring != NULL)
                    setenv("QUERY_STRING", querystring, TRUE);
                
                printf("HTTP/1.1 200 OK\n");
                execl(route, route, NULL);
                printf("Content-Type: text/html\n\n<h1>CGI not found</h1>");
            }
            exit(0);
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

void not_found() {
    printf("HTTP/1.1 404 NOT FOUND\nContent-Type: text/html\n\n<h1>Not Found</h1>");
    exit(0);
}