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
#define MAX_USER 30

struct USER {
    int         id;
    char        *name;
    char        *path;
    char        *ip;
    int         port;
    int         pipes[MAX_LINE][2];
    int         current_line;
    int         connfd;
};

void showSymbol(int sockfd);
int receive_cmd(struct USER *user);
int parse_line(char *input, char* linv[]);
int parse_cmd(char input[], char *out_cmd[]);
int parse_argv(char input[], char *out_argv[]);
int parse_number(char input[]);
struct USER* set_new_user(int connfd, struct sockaddr_in *cliaddr);
void broadcast(char *message, size_t length);

struct USER* get_user(int connfd);

char fork_process(char *argv[], int fd_in, int fd_out, int fd_errout, int sockfd);

struct USER users[30];

char welcome[] = "****************************************\n** Welcome to the information server. **\n****************************************\n% ";

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
    
    fd_set rfds; /* read file descriptor set */
    fd_set afds; /* active file descriptor set */
    int nfds = 4;
    FD_ZERO(&afds);
    FD_SET(listenfd, &afds);
    
    for (;;) {
        memcpy(&rfds, &afds, sizeof(rfds));

        Select(nfds, &rfds, NULL, NULL, NULL);
        
        if (FD_ISSET(listenfd, &rfds)) {
            clilen = sizeof(cliaddr);
            connfd = Accept(listenfd, (SA *) &cliaddr, &clilen);
            FD_SET(connfd, &afds);

            if (connfd + 1 > nfds)
                nfds = connfd + 1;

            Writen(connfd, welcome, sizeof(welcome) - 1);

            struct USER* user = set_new_user(connfd, &cliaddr);
            
            char notify_buf[100];
            int len = sprintf(notify_buf, "*** User '%s' entered from %s/%d. ***\n", user->name, user->ip, user->port);
            broadcast(notify_buf, len);
        }
        for (int fd = 0; fd<nfds; fd++) {
            if (fd != listenfd && FD_ISSET(fd, &rfds)) {
                struct USER *user = get_user(fd);
                if (receive_cmd(user) == -1) { //exit
                    // TODO: update nfds

                    char notify_buf[100];
                    int len = sprintf(notify_buf, "*** User '%s' left. ***\n", user->name);
                    broadcast(notify_buf, len);
                    
                    user->id = 0;
                    free(user->ip);
                    free(user->path);
                    free(user->name);
                    
                    Close(fd);
                    FD_CLR(fd, &afds);
                }
            }
        }
    }

    return 0;
}

struct USER* set_new_user(int connfd, struct sockaddr_in *cliaddr) {
    for (int i = 0; i < MAX_USER; i++) {
        struct USER* user = &users[i];
        if (user->id == 0) {
            user->id = i + 1;
            user->connfd = connfd;
            user->ip = Strdup(inet_ntoa(cliaddr->sin_addr));
            user->port = cliaddr->sin_port;
            user->current_line = 0;
            user->path = Strdup("/bin:bin:.");
            user->name = Strdup("(no name)");
            memset(user->pipes, -1, sizeof(user->pipes));
            return user;
        }
    }
    return NULL;
}

struct USER* get_user(int connfd) {
    for (int i = 0; i < MAX_USER; i++)
        if (users[i].connfd == connfd)
            return &users[i];
    return NULL;
}

void broadcast(char *message, size_t length) {
    if (length == 0) length = strlen(message);
    for (int i = 0; i < MAX_USER; i++) {
        if (users[i].id > 0)
            Writen(users[i].connfd, message, length);
    }
}


int receive_cmd(struct USER *user)
{
    ssize_t     n = 0;
    char        buf[MAX_BUFF];

    setenv("PATH", user->path, TRUE);

    int pos = 0;
    int unknown_command = 0;

    do {
        n = Read(user->connfd, &buf[pos], MAX_BUFF - pos);
        pos += n;
    } while (buf[pos - 1] != '\n');
    buf[pos] = '\0';
    
    char *linv[MAX_LINE];
    int linc = parse_line(buf, linv);
    
    for (int z = 0; z < linc; z++) {
        char *cmdv[MAX_CMDS];
        int cmdc = parse_cmd(linv[z], cmdv);
        printf("line: %d\n unknwon: %d\n", user->current_line, unknown_command);
        int fd_in = user->pipes[user->current_line][0];
        
        if (user->pipes[user->current_line][1] != -1 && !unknown_command) {
            Close(user->pipes[user->current_line][1]);
            user->pipes[user->current_line][1] = -1;
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
                return -1;
            
            if (strcmp(argv[0], "printenv") == 0) {
                for (int j = 1; j < argc; j++)
                    dprintf(user->connfd, "%s=%s\n", argv[j], getenv(argv[j]));
                break;
            }
            
            if (strcmp(argv[0], "setenv") == 0) {
                if (argc == 3) {
                    setenv(argv[1], argv[2], TRUE);
                    if (strcmp(argv[1], "PATH") == 0) {
                        user->path = Strdup(argv[2]);
                    }
                }
                else
                    dprintf(user->connfd, "usage: setenv KEY VALIE\n");
                break;
            }
            
            if (strcmp(argv[0], "who") == 0) {
                dprintf(user->connfd, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
                for (int b = 0; b < MAX_USER; b++) {
                    if (users[b].id > 0) {
                        dprintf(user->connfd, "%d\t%s\t%s/%d\t%s\n",
                                users[b].id,
                                users[b].name,
                                users[b].ip,
                                users[b].port,
                                users[b].connfd == user->connfd ? "<-me":"");
                    }
                }
                break;
            }
            
            if (strcmp(argv[0], "name") == 0) {
                for (int b = 0; b < MAX_USER; b++) {
                    if (users[b].id > 0) {
                        if (strcmp(argv[1], users[b].name) == 0) {
                            dprintf(user->connfd, "*** User '%s' already exists. ***\n", argv[1]);
                            return 0;
                        }
                    }
                }
                free(user->name);
                user->name = Strdup(argv[1]);
                char notify_buffer[100];
                int len = sprintf(notify_buffer, "*** User from %s/%d is named '%s'. ***\n", user->ip, user->port, argv[1]);
                broadcast(notify_buffer, len);
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
                fd_out = user->connfd;
                fd_errout = user->connfd;
                
                int minus = 0;
                
                for (int q = 0; q < argc; q++) {
                    if (strcmp(argv[q], ">") == 0) {
                        fd_out = open(argv[q + 1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                        argv[q] = '\0';
                        argc = q;
                        break;
                    } else if (argv[q][0] == '|' && argv[q][1] != '!') {
                        int dest_pipe = parse_number(&argv[q][1]) + user->current_line;
                        printf("dest_pipe std = %d\n", dest_pipe);
                        if (user->pipes[dest_pipe][1] == -1)
                            Pipe(user->pipes[dest_pipe]);
                        fd_out = user->pipes[dest_pipe][1];
                        close_fd_out = 0;
                        
                        argv[q] = '\0';
                        minus++;
                    } else if (argv[q][0] == '!' && argv[q][1] != '|') {

                        int dest_pipe = parse_number(&argv[q][1]) + user->current_line;
                        printf("dest_pipe err = %d\n", dest_pipe);
                        
                        if (user->pipes[dest_pipe][1] == -1)
                            Pipe(user->pipes[dest_pipe]);
                        fd_errout = user->pipes[dest_pipe][1];
                        close_fd_errout = 0;

                        argv[q] = '\0';
                        minus++;
                    } else if (
                               (argv[q][0] == '!' && argv[q][1] == '|') ||
                               (argv[q][0] == '|' && argv[q][1] == '!')) {

                        int dest_pipe = atoi(&argv[q][2]) + user->current_line;
                        printf("dest_pipe std+err = %d\n", dest_pipe);
                        
                        if (user->pipes[dest_pipe][1] == -1)
                            Pipe(user->pipes[dest_pipe]);

                        fd_out = user->pipes[dest_pipe][1];
                        fd_errout = user->pipes[dest_pipe][1];
                        
                        
                        argv[q] = '\0';
                        minus++;
                    }
                }
                argc -= minus;
            }
            
            printf("pipe[0]=%d\n", in_out_pipe[0]);
            printf("pipe[1]=%d\n", in_out_pipe[1]);
            
            char exit_code = fork_process(argv, fd_in, fd_out, fd_errout, user->connfd);
            
            if (close_fd_out && fd_out != user->connfd && fd_out != -1)
                Close(fd_out);
            if (close_fd_errout && fd_errout != fd_out && fd_errout != user->connfd && fd_errout != -1)
                Close(fd_errout);

            if (exit_code == ERR_CMD_NOT_FOUND) {
                dprintf(user->connfd, "Unknown command: [%s].\n", argv[0]);
                unknown_command = 1;
                break;
            } else {
                if (fd_in != -1)
                    Close(fd_in);
                fd_in = in_out_pipe[0];
            }
            
        }

        showSymbol(user->connfd);

        if (!unknown_command)
            user->current_line++;
    }
    return 0;
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

int parse_number(char input[]) {
    char *delim = "+";
    
    char *p = strtok(input, delim);
    
    if (p == NULL)
        return 0;
    
    int num = atoi(p);

    while ((p = strtok(NULL, delim)) != NULL) {
        num += atoi(p);
    }
    
    return num;
}
