//
//  main.c
//  homework1
//
//  Created by 傑瑞 on 2015/10/26.
//  Copyright © 2015年 傑瑞. All rights reserved.
//

#include <stdio.h>
#include "../lib/unp.h"

#define MAX_BUFF 100000
#define MAX_LINE 40000
#define MAX_CMDS 10000
#define MAX_ARGS 20
#define ERR_CMD_NOT_FOUND -5
#define TRUE 1
#define FALSE 0

void showSymbol(int sockfd);
void receive_cmd(int sockfd);
int parse_line(char *input, char* linv[]);
int parse_cmd(char input[], char *out_cmd[]);
int parse_argv(char input[], char *out_argv[]);
char fork_process(char *argv[], int fd_in, int fd_out, int fd_errout, int sockfd);

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

char welcome[] = "****************************************\n** Welcome to the information server. **\n****************************************\n% ";

void receive_cmd(int sockfd)
{
    Writen(sockfd, welcome, sizeof(welcome) - 1);

    ssize_t     n = 0;
    char        buf[MAX_BUFF];
    int         pipes[MAX_LINE][2];
    int         line = 0;
    
    memset(pipes, -1, sizeof(pipes));

    setenv("PATH", "/Users/Fonger/ras/bin:bin:.", TRUE);
    printf("%s\n" ,getenv("PATH"));
    int pos = 0;
    int unknown_command = 0;
again:
    while ( (n = read(sockfd, &buf[pos], MAX_BUFF - pos)) > 0) {
        if (buf[pos + n - 1] != '\n') {
            printf("n-1 != newline...\n");
            printf("n-1 is (%c), (%x)\n", buf[n-1], buf[n-1]);
            pos += n;
            goto again;
        } else {
            buf[pos + n] = '\0';
            pos = 0;
            printf("I see newline!!\n");
        }
    
        char *linv[MAX_LINE];
        int linc = parse_line(buf, linv);
        
        for (int z = 0; z < linc; z++) {
            char *cmdv[MAX_CMDS];
            int cmdc = parse_cmd(linv[z], cmdv);
            printf("line: %d\n unknwon: %d\n", line, unknown_command);
            int fd_in = pipes[line][0];
            
            if (pipes[line][1] != -1 && !unknown_command) {
                Close(pipes[line][1]);
                pipes[line][1] = -1;
            }
            
            unknown_command = 0;
            
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
                
                int fd_out;
                int fd_errout;
                int in_out_pipe[2];

                int close_fd_out = 1;
                int close_fd_errout = 1;
                
                if (i + 1 < cmdc) {
                    // If there's next command
                    Pipe(in_out_pipe);
                    fd_out = in_out_pipe[1];
                    fd_errout = in_out_pipe[1];
                } else {
                    // This is last one
                    fd_out = sockfd;
                    fd_errout = sockfd;
                    
                    int minus = 0;
                    
                    for (int q = 0; q < argc; q++) {
                        if (strcmp(argv[q], ">") == 0) {
                            fd_out = open(argv[q + 1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                            argv[q] = '\0';
                            argc = q;
                            break;
                        } else if (argv[q][0] == '|' && argv[q][1] != '!') {
                            int dest_pipe = atoi(&argv[q][1]) + line;
                            printf("dest_pipe std = %d\n", dest_pipe);
                            if (pipes[dest_pipe][1] == -1)
                                Pipe(pipes[dest_pipe]);
                            fd_out = pipes[dest_pipe][1];
                            close_fd_out = 0;
                            
                            argv[q] = '\0';
                            minus++;
                        } else if (argv[q][0] == '!' && argv[q][1] != '|') {

                            int dest_pipe = atoi(&argv[q][1]) + line;
                            printf("dest_pipe err = %d\n", dest_pipe);
                            
                            if (pipes[dest_pipe][1] == -1)
                                Pipe(pipes[dest_pipe]);
                            fd_errout = pipes[dest_pipe][1];
                            close_fd_errout = 0;

                            argv[q] = '\0';
                            minus++;
                        } else if (
                                   (argv[q][0] == '!' && argv[q][1] == '|') ||
                                   (argv[q][0] == '|' && argv[q][1] == '!')) {

                            int dest_pipe = atoi(&argv[q][2]) + line;
                            printf("dest_pipe std+err = %d\n", dest_pipe);
                            
                            if (pipes[dest_pipe][1] == -1)
                                Pipe(pipes[dest_pipe]);

                            fd_out = pipes[dest_pipe][1];
                            fd_errout = pipes[dest_pipe][1];
                            
                            
                            argv[q] = '\0';
                            minus++;
                        }
                    }
                    argc -= minus;
                }
                
                printf("pipe[0]=%d\n", in_out_pipe[0]);
                printf("pipe[1]=%d\n", in_out_pipe[1]);
                
                char exit_code = fork_process(argv, fd_in, fd_out, fd_errout, sockfd);
                
                if (close_fd_out && fd_out != sockfd && fd_out != -1)
                    Close(fd_out);
                if (close_fd_errout && fd_errout != fd_out && fd_errout != sockfd && fd_errout != -1)
                    Close(fd_errout);

                if (exit_code == ERR_CMD_NOT_FOUND) {
                    dprintf(sockfd, "Unknown command: [%s].\n", argv[0]);
                    unknown_command = 1;
                    break;
                } else {
                    if (fd_in != -1)
                        Close(fd_in);
                    fd_in = in_out_pipe[0];
                }
                
            }

            showSymbol(sockfd);

            if (!unknown_command)
                line++;
        }
    }
    if (n < 0 && errno == EINTR) {
        pos += n;
        goto again;
    }
    else if (n < 0) {
        err_sys("str_echo: read error");
    }

}


char fork_process(char *argv[], int fd_in, int fd_out, int fd_errout, int sockfd) {

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
        if (fd_in != -1) {
            Dup2(fd_in, STDIN_FILENO);
            Close(fd_in);
        }

        Dup2(fd_out, STDOUT_FILENO);
        Dup2(fd_errout, STDERR_FILENO);
        
        Close(fd_out);

        if (fd_out != fd_errout)
            Close(fd_errout);
        
        execvp(argv[0], argv);
        
        exit(ERR_CMD_NOT_FOUND);
    } else {
        err_sys("fork failed");
    }
    return 0;
}
void showSymbol(int sockfd) {
    Writen(sockfd, "% ", 2);
}
int parse_line(char *input, char* linv[]) {
    char *delim = "\r\n";
    
    char *p = strtok(input, delim);
    
    if (p == NULL)
        return 0;
    
    int  linc = 0;
    linv[linc++] = p;
    while ((p = strtok(NULL, delim)) != NULL)
        linv[linc++] = p;
    linv[linc] = '\0';
    return linc;
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
