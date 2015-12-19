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

#define ERR_CGI_NOT_FOUND -5
#define TRUE 1
#define FALSE 0



int receive_cmd();

char fork_process(char *cgi, int sockfd);


char fifo_dir[] = "/tmp/nphw2-0112503/";
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

            Dup2(connfd, STDOUT_FILENO);
            printf("HTTP/1.1 200 OK\nContent-Type: text/html\n\n<b>helloworld</b>");
            Close(connfd);
            
            exit(0);
        }
    }
    return 0;
}

char fork_process(char *cgi, int sockfd) {

    pid_t child_pid = Fork();
    if (child_pid > 0) { // parent

        printf("Child spawn with pid: %d\n", child_pid);
        
        int status = 0;
        while( (wait( &status ) == -1) && (errno == EINTR) );

        if (WIFEXITED(status)) {
            char exit_code = WEXITSTATUS(status);
            printf("Child pid (%d) exit code: %d\n", child_pid, exit_code);
            return exit_code;
        } else {
            printf("Child pid (%d) crash status: %x\n", child_pid, status);
            return 0;
        }
        
    } else if (child_pid == 0) { // child

        Dup2(sockfd, STDOUT_FILENO);
        Close(sockfd);
        execl(cgi, cgi, NULL);
        
        exit(ERR_CGI_NOT_FOUND);
    } else {
        err_sys("fork failed");
    }
    return 0;
}

