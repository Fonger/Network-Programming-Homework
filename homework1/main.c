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
#define MAX_LINE 40000
#define MAX_CMDS 10000
#define MAX_ARGS 20
#define MAX_NAME 20
#define ERR_CMD_NOT_FOUND -5
#define TRUE 1
#define FALSE 0
#define MAX_USER 30
#define MAX_CHAT 2000

struct USER {
    int         id;
    char        name[MAX_NAME + 1];
    char        ip[INET6_ADDRSTRLEN];
    int         port;
    pid_t       pid;
    char        msg[MAX_CHAT];
};

void showSymbol(int sockfd);
int receive_cmd();
int parse_cmd(char input[], char *out_cmd[]);
int parse_argv(char input[], char *out_argv[]);
int parse_number(char input[]);
struct USER* set_new_user(struct sockaddr_in *cliaddr);
void clear_user();
void broadcast(const char *format, ...);

struct USER* get_user(int id);

char fork_process(char *argv[], int fd_in, int fd_out, int fd_errout, int sockfd);

int shmid_user;
key_t shm_key_user = 0x19931009;
struct USER *users;

int shmid_public_msg;
key_t shm_key_public_msg = 0x19931010;
char *public_msg;

int public_pipes[101][2] = { -1 };

char welcome[] = "****************************************\n** Welcome to the information server. **\n****************************************\n% ";

int child_connfd = -1;
struct USER *me;

static void intrupt_handler(int signo) {
    if (shmid_user > 0) {
        printf("[%d] Detaching shared memory of users...\n", getpid());
        if (users != NULL && shmdt(users) < 0)
            err_sys("shmdt users");
        
        /* if master */
        if (child_connfd < 0) {
            printf("[%d] Master is removing shared memory of users...\n", getpid());
            shmctl(shmid_user, IPC_RMID, NULL);
        }
    }
    if (shmid_public_msg > 0) {
        printf("[%d] Detaching shared memory of public msg...\n", getpid());
        if (public_msg != NULL && shmdt(public_msg) < 0)
            err_sys("shmdt public_msg");
        
        /* if master */
        if (child_connfd < 0) {
            printf("[%d] Master is removing shared memory of public msg...\n", getpid());
            shmctl(shmid_public_msg, IPC_RMID, NULL);
        }
    }
    exit(0);
}

static void broadcast_handler(int signo) {
    if (child_connfd > 0) {
        Writen(child_connfd, public_msg, strlen(public_msg));
    }
}

static void tell_handler(int signo) {
    if (child_connfd > 0) {
        Writen(child_connfd, me->msg, strlen(me->msg));
    }
}
int main() {
    printf("Hello, World!\n");
    shmid_user = 0;

    shmid_user = shmget(shm_key_user, MAX_USER * sizeof(struct USER), SHM_R | SHM_W | IPC_CREAT);
    if (shmid_user < 0)
        err_sys("shmget users");
    
    users = shmat(shmid_user, NULL, 0);
    if (users < 0)
        err_sys("shmat users");
    
    bzero(users, MAX_USER * sizeof(struct USER));

    shmid_public_msg = 0;
    
    shmid_public_msg = shmget(shm_key_public_msg, MAX_CHAT, SHM_R | SHM_W | IPC_CREAT);
    if (shmid_public_msg < 0)
        err_sys("shmget public_msg");
    
    public_msg = shmat(shmid_public_msg, NULL, 0);
    if (public_msg < 0)
        err_sys("shmat public_msg");
    
    if (signal(SIGINT, intrupt_handler) == SIG_ERR)
        err_sys("Setting signal SIGINT");

    if (signal(SIGUSR1, broadcast_handler) == SIG_ERR)
        err_sys("Setting signal SIGUSR1");
    
    if (signal(SIGUSR2, tell_handler) == SIG_ERR)
        err_sys("Setting signal SIGUSR2");
    
    //chdir("/Users/jerry/Downloads/ras");
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
            Close(connfd);
        } else if (child_pid == 0) {
            /* child */
            child_connfd = connfd;
            Close(listenfd);
            Writen(connfd, welcome, sizeof(welcome) - 1);
            if (set_new_user(&cliaddr) == NULL) {
                dprintf(connfd, "Sorry, the server is over capicity (%d/%d).\n", MAX_USER, MAX_USER);
                goto fail;
            }
            broadcast("*** User '%s' entered from %s/%d. ***\n", me->name, me->ip, me->port);
            printf("new user id=%d\n", me->id);
            receive_cmd();
            broadcast("*** User '%s' left. ***\n", me->name);
            clear_user();
        fail:
            Close(connfd);
            raise(SIGINT);
        }
    }
    return 0;
}

struct USER* set_new_user(struct sockaddr_in *cliaddr) {
    for (int i = 0; i < MAX_USER; i++) {
        struct USER* user = &users[i];
        if (user->pid == 0) {
            user->id = i + 1;
            user->pid = getpid();
            user->port = cliaddr->sin_port;
            strcpy(user->ip, inet_ntoa(cliaddr->sin_addr));
            strcpy(user->name, "(no name)");
            me = user;
            return user;
        }
    }
    return NULL;
}

void clear_user() {
    me->pid = 0;
}

struct USER* get_user(int id) {
    if (id > 0 && id < MAX_USER) {
        struct USER* user = &users[id - 1];
        if (user->pid > 0)
            return user;
    }
    return NULL;
}

void broadcast(const char *format, ...) {
    va_list args;

    va_start(args, format);
    vsnprintf(public_msg, MAX_CHAT, format, args);
    va_end(args);

    for (int i = 0; i < MAX_USER; i++) {
        if (users[i].id > 0) {
            kill(users[i].pid, SIGUSR1);
        }
    }
}

int receive_cmd()
{
    setenv("PATH", "/Users/jerry/Downloads/ras/bin:/bin:bin:.", TRUE);

    ssize_t     n = 0;
    char        buf[MAX_BUFF];
    int         pos;
    int         unknown_command = 0;
    int         pipes[MAX_LINE][2];
    int         current_line;
    
    memset(pipes, -1, sizeof(pipes));

    for(;;) {
        pos = 0;
        do {
            n = Read(child_connfd, &buf[pos], MAX_BUFF - pos);
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
        printf("line: %d\n unknwon: %d\n", current_line, unknown_command);
        int fd_in = pipes[current_line][0];
        
        if (pipes[current_line][1] != -1 && !unknown_command) {
            Close(pipes[current_line][1]);
            pipes[current_line][1] = -1;
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
                    dprintf(child_connfd, "%s=%s\n", argv[j], getenv(argv[j]));
                break;
            }
            
            if (strcmp(argv[0], "setenv") == 0) {
                if (argc == 3)
                    setenv(argv[1], argv[2], TRUE);
                else
                    dprintf(child_connfd, "usage: setenv KEY VALIE\n");
                break;
            }
            
            if (strcmp(argv[0], "who") == 0) {
                dprintf(child_connfd, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
                for (int b = 0; b < MAX_USER; b++) {
                    if (users[b].pid > 0) {
                        dprintf(child_connfd, "%d\t%s\t%s/%d\t%s\n",
                                users[b].id,
                                users[b].name,
                                users[b].ip,
                                users[b].port,
                                users[b].id == me->id ? "<-me":"");
                    }
                }
                break;
            }
            
            if (strcmp(argv[0], "name") == 0) {
                if (argc < 2) {
                    dprintf(child_connfd, "usage: name (name)\n");
                    break;
                }
                
                for (int j = 2; j < argc; j++)
                    *(argv[j] - 1) = ' ';
                
                for (int b = 0; b < MAX_USER; b++) {
                    if (users[b].id > 0) {
                        if (strcmp(argv[1], users[b].name) == 0) {
                            dprintf(child_connfd, "*** User '%s' already exists. ***\n%% ", argv[1]);
                            free(input);
                            return 0;
                        }
                    }
                }

                strlcpy(me->name, argv[1], sizeof(me->name));
                broadcast("*** User from %s/%d is named '%s'. ***\n", me->ip, me->port, argv[1]);
                break;
            }
            
            if (strcmp(argv[0], "yell") == 0) {
                if (argc < 2) {
                    dprintf(child_connfd, "usage: yell (message)\n");
                    break;
                }
                
                for (int j = 2; j < argc; j++)
                    *(argv[j] - 1) = ' ';

                broadcast("*** %s yelled ***: %s\n", me->name, argv[1]);
                break;
            }
            
            if (strcmp(argv[0], "tell") == 0) {
                if (argc < 3) {
                    dprintf(child_connfd, "usage: tell (client id) (message)\n");
                    break;
                }
                for (int j = 3; j < argc; j++)
                    *(argv[j] - 1) = ' ';
                
                int dest_user_id = atoi(argv[1]);

                struct USER* dest_user = get_user(dest_user_id);
                if (dest_user == NULL) {
                    dprintf(child_connfd, "*** Error: user #%d does not exist yet. ***\n", dest_user_id);
                } else {
                    snprintf(dest_user->msg, sizeof(dest_user->msg), "*** %s told you ***: %s\n", me->name, argv[2]);
                    kill(dest_user->pid, SIGUSR2);
                }
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
                            dprintf(child_connfd, "*** Error: public pipe #%d does not exist yet. ***\n%% ", pipe_id);
                            return 0;
                        }
                        Close(pub_pipe[1]);
                        pub_pipe[1] = -1;
                        fd_in = pub_pipe[0];
                        
                        broadcast("*** %s (#%d) just received via '%s' ***\n", me->name, me->id, input);
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
                fd_out = child_connfd;
                fd_errout = child_connfd;
                
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
                            dprintf(child_connfd, "*** Error: public pipe #%d already exists. ***\n%% ", pipe_id);
                            return 0;
                        }
                        fd_out = pub_pipe[1];
                        fd_errout = pub_pipe[1];
                        close_fd_out = 0;
                        broadcast("*** %s (#%d) just piped '%s' ***\n", me->name, me->id, input);
                    } else if (argv[q][0] == '|' && argv[q][1] != '!') {
                        int dest_pipe = parse_number(&argv[q][1]) + current_line;
                        printf("dest_pipe std = %d\n", dest_pipe);
                        if (pipes[dest_pipe][1] == -1)
                            Pipe(pipes[dest_pipe]);
                        fd_out = pipes[dest_pipe][1];
                        close_fd_out = 0;
                        
                        argv[q] = '\0';
                        minus++;
                    } else if (argv[q][0] == '!' && argv[q][1] != '|') {

                        int dest_pipe = parse_number(&argv[q][1]) + current_line;
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

                        int dest_pipe = atoi(&argv[q][2]) + current_line;
                        printf("dest_pipe std+err = %d\n", dest_pipe);
                        
                        if (pipes[dest_pipe][1] == -1)
                            Pipe(pipes[dest_pipe]);

                        fd_out = pipes[dest_pipe][1];
                        fd_errout = pipes[dest_pipe][1];
                        
                        
                        argv[q] = '\0';
                        minus++;
                    }
                }
            }
            
            argc -= minus;
            
            printf("pipe[0]=%d\n", in_out_pipe[0]);
            printf("pipe[1]=%d\n", in_out_pipe[1]);
            
            char exit_code = fork_process(argv, fd_in, fd_out, fd_errout, child_connfd);
            
            if (close_fd_out && fd_out != child_connfd && fd_out != -1)
                Close(fd_out);
            if (close_fd_errout && fd_errout != fd_out && fd_errout != child_connfd && fd_errout != -1)
                Close(fd_errout);

            if (exit_code == ERR_CMD_NOT_FOUND) {
                dprintf(child_connfd, "Unknown command: [%s].\n", argv[0]);
                unknown_command = 1;
                break;
            } else {
                if (fd_in != -1)
                    Close(fd_in);
                fd_in = in_out_pipe[0];
            }
            
        }

        showSymbol(child_connfd);

        if (!unknown_command)
            current_line++;
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
