//
//  main.c
//  homework1
//
//  Created by 傑瑞 on 2015/10/26.
//  Copyright © 2015年 傑瑞. All rights reserved.
//

#include <stdio.h>
#include "unp.h"

#define MAX_LINE 15000
#define MAX_CMDS 2500
#define MAX_ARGS 10

void showSymbol(int sockfd);
void receive_cmd(int sockfd);
int parse_cmd(char input[], char *out_cmd[]);
int parse_argv(char input[], char *out_argv[]);
int fork_process(char *argv[], int toMaster[2]);

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
    ssize_t     n;
    char        buf[MAX_LINE];
    char        *cmdv[MAX_CMDS];

    setenv("PATH", "/Users/Fonger/ras/bin:/bin:.", TRUE);

again:
    while ( (n = read(sockfd, buf, MAX_LINE)) > 0) {
        buf[n] = '\0';

        
        int cmdc = parse_cmd(buf, cmdv);
        int toChild[2] = { -1, -1 };
        for (int i = 0; i < cmdc; i++) {
            printf("cmdv[%d] = %s\n", i, cmdv[i]);

            char *argv[MAX_ARGS];

            int argc = parse_argv(cmdv[i], argv);
            
            if (argc == 0)
                continue;
            
            for (int j = 0; j < argc; j++)
                printf("argv[%d] = %s\n", j, argv[j]);
            
            if (strcmp(argv[0], "exit") == 0)
                return;
            
            if (strcmp(argv[0], "printenv") == 0) {
                for (int j = 1; j < argc; j++)
                    dprintf(sockfd, "%s=%s\n", argv[j], getenv(argv[j]));
                break;
            }
            
            if (strcmp(argv[0], "setenv") == 0) {
                if (argc == 3)
                    setenv(argv[1], argv[2], TRUE);
                else
                    dprintf(sockfd, "usage: setenv KEY VALIE\n");
                break;
            }

            int result_fd = fork_process(argv, toChild);

            Pipe(toChild);
            ssize_t z;
            char result[1000];
        agz:
            while ( (z = read(result_fd, result, sizeof(z)) ) > 0) {
                if (i != cmdc - 1) {
                    Writen(toChild[1], result, z);
                } else {
                    Writen(sockfd, result, z);
                }
            }
            if (z < 0 && errno == EINTR)
                goto agz;
            Close(toChild[1]);
            Close(result_fd);
        }

        showSymbol(sockfd);
    }
    if (n < 0 && errno == EINTR) {
        goto again;
    }
    else if (n < 0) {
        err_sys("str_echo: read error");
    }
}


int fork_process(char *argv[], int toChild[2]) {
    int toMaster[2];
    Pipe(toMaster);
    
    pid_t child_pid = Fork();
    if (child_pid > 0) { // parent
        if (toChild[0] != -1)
            Close(toChild[0]);
        Close(toMaster[1]);
        printf("Child spawn with pid: %d\n", child_pid);
        
        int status = 0;
        while( (wait( &status ) == -1) && (errno == EINTR) );

        printf("QQ %d \n", toChild[1]);
        return toMaster[0];
    } else if (child_pid == 0) { // child
        if (toChild[0] != -1) {
            Dup2(toChild[0], STDIN_FILENO);
            Close(toChild[0]);
        }
        Dup2(toMaster[1], STDOUT_FILENO);
        Close(toMaster[1]);
        
        execvp(argv[0], argv);
        
        printf("Unknown Command: [%s]\n", argv[0]);
        exit(-1);
    } else {
        err_sys("fork failed");
    }
    return -1;
}
void showSymbol(int sockfd) {
    char *symbol = "\n% ";
    Writen(sockfd, symbol, 3);
}

int parse_cmd(char *input, char *cmd[]) {
    char    *symbol = " | ";
    size_t  length   = strlen(input);
    size_t  offset   = strlen(symbol);
    
    if (length == 0)
        return 0;
    
    if (input[length - 1] == '\n') {
        if (--length == 0) // if only input \n
            return 0;
        input[length] = '\0';
    }
    
    
    int i = 0;
    cmd[0] = input;

    for (i = 1; (input = strstr(input, symbol)) != NULL; i++) {
        *(input) = '\0';
        input += offset;
        cmd[i] = input;
    }
    return i;
}

int parse_argv(char input[], char *argv[]) {
    char *delim = " \n";
    
    char *p = strtok(input, delim);
    
    if (p == NULL)
        return 0;
    
    int  argc = 0;
    argv[argc++] = p;
    while ((p = strtok(NULL, delim)) != NULL)
        argv[argc++] = p;
    argv[argc] = '\0';
    return argc;
}