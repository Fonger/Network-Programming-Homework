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
int parse_cmd(char input[], char *out_cmd[]);
int parse_argv(char input[], char *out_argv[]);
int parse_number(char input[]);
struct USER* set_new_user(int connfd, struct sockaddr_in *cliaddr);
void clear_user(struct USER* user);
void broadcast(const char *format, ...);

struct USER* get_user(int connfd);

char fork_process(char *argv[], int fd_in, int fd_out, int fd_errout, int sockfd);

struct USER users[30];
int public_pipes[101][2] = { -1 };

char welcome[] = "****************************************\n** Welcome to the information server. **\n****************************************\n% ";

int main() {
    printf("Hello, World!\n");
    chdir("/Users/jerry/Downloads/ras");
    memset(public_pipes, -1, sizeof(public_pipes));
    
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
        
        pid_t child_pid = Fork();
        if (child_pid > 0) {

        } else if (child_pid == 0) {
            /* child */
            Close(listenfd);
            Writen(connfd, welcome, sizeof(welcome) - 1);
            struct USER *user = set_new_user(connfd, &cliaddr);
            receive_cmd(user);
            exit(EXIT_SUCCESS);
        }
        Close(connfd);
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
            user->path = Strdup("/Users/jerry/Downloads/ras/bin:/bin:bin:.");
            user->name = Strdup("(no name)");
            memset(user->pipes, -1, sizeof(user->pipes));
            return user;
        }
    }
    return NULL;
}

void clear_user(struct USER* user) {
    user->id = 0;
    free(user->ip);
    free(user->path);
    free(user->name);
}

struct USER* get_user(int connfd) {
    for (int i = 0; i < MAX_USER; i++)
        if (users[i].connfd == connfd)
            return &users[i];
    return NULL;
}

void broadcast(const char *format, ...) {
    va_list args;
    
    for (int i = 0; i < MAX_USER; i++) {
        if (users[i].id > 0) {
            va_start(args, format);
            vdprintf(users[i].connfd, format, args);
            va_end(args);
        }
    }
}


int receive_cmd(struct USER *user)
{
    for(;;) {
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
        
        char *input = Strdup(buf);
        if (input[pos-2] == '\r')
            input[pos-2] = '\0';
        if (input[pos-1] == '\n')
            input[pos-1] = '\0';

        
        char *cmdv[MAX_CMDS];
        int cmdc = parse_cmd(buf, cmdv);
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
            
            if (strcmp(argv[0], "exit") == 0) {
                free(input);
                return -1;
            }
            
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
                if (argc < 2) {
                    dprintf(user->connfd, "usage: name (name)\n");
                    break;
                }
                
                for (int j = 2; j < argc; j++)
                    *(argv[j] - 1) = ' ';
                
                for (int b = 0; b < MAX_USER; b++) {
                    if (users[b].id > 0) {
                        if (strcmp(argv[1], users[b].name) == 0) {
                            dprintf(user->connfd, "*** User '%s' already exists. ***\n", argv[1]);
                            free(input);
                            return 0;
                        }
                    }
                }
                free(user->name);
                user->name = Strdup(argv[1]);
                broadcast("*** User from %s/%d is named '%s'. ***\n", user->ip, user->port, argv[1]);
                break;
            }
            
            if (strcmp(argv[0], "yell") == 0) {
                if (argc < 2) {
                    dprintf(user->connfd, "usage: yell (message)\n");
                    break;
                }
                
                for (int j = 2; j < argc; j++)
                    *(argv[j] - 1) = ' ';

                broadcast("*** %s yelled ***: %s\n", user->name, argv[1]);
                break;
            }
            
            if (strcmp(argv[0], "tell") == 0) {
                if (argc < 3) {
                    dprintf(user->connfd, "usage: tell (client id) (message)\n");
                    break;
                }
                for (int j = 3; j < argc; j++)
                    *(argv[j] - 1) = ' ';
                
                int dest_user_id = atoi(argv[1]);
                struct USER* dest_user = &users[dest_user_id - 1];
                
                if (dest_user->id > 0)
                    dprintf(dest_user->connfd, "*** %s told you ***: %s\n", user->name, argv[2]);
                else
                    dprintf(user->connfd, "*** Error: user #%d does not exist yet. ***\n", dest_user_id);
                break;
            }
            
            int fd_out;
            int fd_errout;
            int in_out_pipe[2];

            int close_fd_out = 1;
            int close_fd_errout = 1;
            
            int minus = 0;

            // Receiving from public pipe only happen in first command
            if (i == 0) {
                for (int q = 0; q < argc; q++) {
                    if (argv[q][0] == '<') {
                        int pipe_id = atoi(&argv[q][1]);
                        int *pub_pipe = public_pipes[pipe_id];
                        minus++;
                        argv[q] = '\0';
                        if (pub_pipe[1] == -1) {
                            dprintf(user->connfd, "*** Error: the pipe #%d does not exist yet. ***\n%% ", pipe_id);
                            return 0;
                        }
                        Close(pub_pipe[1]);
                        pub_pipe[1] = -1;
                        fd_in = pub_pipe[0];
                        
                        broadcast("*** %s (#%d) just received via '%s' ***\n", user->name, user->id, input);
                        break;
                    }
                }
            }
            if (i + 1 < cmdc) {
                // If there's next command
                Pipe(in_out_pipe);
                fd_out = in_out_pipe[1];
                fd_errout = in_out_pipe[1];
            } else {
                // This is last one
                fd_out = user->connfd;
                fd_errout = user->connfd;
                
                for (int q = 0; q < argc; q++) {
                    if (argv[q] == '\0') continue;
                    if (strcmp(argv[q], ">") == 0) {
                        fd_out = open(argv[q + 1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                        argv[q] = '\0';
                        argc = q;
                        break;
                    } else if (argv[q][0] == '>') {
                        int pipe_id = atoi(&argv[q][1]);
                        int *pub_pipe = public_pipes[pipe_id];
                        minus++;
                        argv[q] = '\0';
                        if (pub_pipe[1] == -1)
                            Pipe(pub_pipe);
                        else {
                            dprintf(user->connfd, "*** Error: the pipe #%d already exists. ***\n%% ", pipe_id);
                            return 0;
                        }
                        fd_out = pub_pipe[1];
                        close_fd_out = 0;
                        broadcast("*** %s (#%d) just piped '%s' ***\n", user->name, user->id, input);
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
            }
            
            argc -= minus;
            
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
        free(input);
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
    char *delim = " \r\n";
    
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
