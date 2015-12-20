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

void bad_request(char *msg);
void home_page();
void not_found();
void serve_file(char *filename, char *content_type);

int main() {
    printf("Hello, World!\n");
    
    chdir("/Users/Fonger/Desktop/square/themeforest-7511722-square-responsive-admin-app-with-angularjs/square-v1.2/dist");
    //chdir("/Users/Fonger/Desktop/HW3 (2)/server_file");
    
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
                bad_request("No method");
            
            url = strtok(NULL, " ");
            
            if (url == NULL || *url == '\0')
                bad_request("No url found");
            
            //GET /test?qq=123 HTTP/1.1
            
            char *route = strtok(url, "?");
            
            if (route == NULL || *route == '\0')
                home_page();
            
            route++; // exclude the first '/' character
            
            if (*route == '\0')
                home_page();
            
            char *querystring;
            querystring = strtok(NULL, "");

            char *ext = strrchr(route, '.');
            
            if (ext == NULL) {
                bad_request("There's no file extension");
            } else if (strncasecmp(ext, ".htm", 4) == 0) {
                serve_file(route, "text/html");
            } else if (strncasecmp(ext, ".css\0", 5) == 0) {
                serve_file(route, "text/css");
            } else if (strncasecmp(ext, ".js\0", 4) == 0) {
                serve_file(route, "text/javascript");
            } else if (strncasecmp(ext, ".jpg\0", 5) == 0) {
                serve_file(route, "image/jpeg");
            } else if (strncasecmp(ext, ".jpeg\0", 6) == 0) {
                serve_file(route, "image/jpeg");
            } else if (strncasecmp(ext, ".png\0", 5) == 0) {
                serve_file(route, "image/png");
            } else if (strncasecmp(ext, ".gif\0", 5) == 0) {
                serve_file(route, "image/gif");
            } else if (strncasecmp(ext, ".bmp\0", 5) == 0) {
                serve_file(route, "image/bmp");
            } else if (strncasecmp(ext, ".ico\0", 5) == 0) {
                serve_file(route, "image/x-icon");
            } else if (strncasecmp(ext, ".woff\0", 6) == 0) {
                serve_file(route, "application/x-font-woff");
            } else if (strncasecmp(ext, ".txt\0", 5) == 0) {
                serve_file(route, "text/plain");
            } else if (strncasecmp(ext, ".cgi\0", 5) == 0){
                setenv("REQUEST_METHOD", method, TRUE);
                
                if (querystring != NULL)
                    setenv("QUERY_STRING", querystring, TRUE);
                
                printf("HTTP/1.1 200 OK\n");
                execl(route, route, NULL);
                printf("Content-Type: text/html\n\n<h1>CGI not found</h1>");
            } else {
                bad_request("Unknown file extension");
            }
            exit(0);
        }
    }
    return 0;
}

void bad_request(char *msg) {
    printf("HTTP/1.1 400 BAD REQUEST\nContent-Type: text/html\n\n<h1>Bad Request</h1>");
    if (msg != NULL)
        printf("<hr><p>%s</p>", msg);
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

void serve_file(char *filename, char *content_type) {
    FILE *file = fopen(filename, "r");
    if (file == NULL)
        not_found();
    
    printf("HTTP/1.1 200 OK\n");
    printf("Content-Type: %s\n\n", content_type);
    
    int c;
    while ((c = getc(file)) != EOF)
        putchar(c);
    fclose(file);
}