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

#define MAX_CLIENT 5
#define TRUE 1
#define FALSE 0

#define F_CONNECTING 0
#define F_READING 1
#define F_WRITING 2
#define F_DONE 3

typedef struct {
    int     index;
    char*   host;
    ushort  port;
    FILE*   batch;
    int     sockfd;
    int     status;
} Client;

int new_client_fd(char *hostname, ushort port);
Client** parse_query_string(char *querystring);
void print_html_frame(Client** clients);
void printc(Client* client, char *format, ...);

int main() {
    chdir("/Users/Fonger/Desktop/HW3 (2)/server_file/test");

    printf("Content-Type: text/html\n\n");
    setenv("QUERY_STRING", "h1=nplinux3.cs.nctu.edu.tw&p1=9877&b1=t1.txt&h2=nplinux3.cs.nctu.edu.tw&p2=9877&b2=t2.txt", 1);
    char *qs = getenv("QUERY_STRING");
    Client* *clients = parse_query_string(qs);

    if (qs == NULL)
        printf("<h1>No QUERY_STRING</h1>\n");
    
    fd_set rfds; /* readable file descriptor set */
    fd_set wfds; /* writable file descriptor set */
    fd_set rfds_a; /* active read file descriptor set */
    fd_set wfds_a; /* active write file descriptor set */
    
    //int nfds = 4;
    int nfds = 9;
    
    FD_ZERO(&rfds_a);
    FD_ZERO(&wfds_a);
    
    for (int i = 0; i < MAX_CLIENT; i++) {
        if (clients[i] == NULL)
            continue;
        int clientfd = new_client_fd(clients[i]->host, clients[i]->port);
        clients[i]->sockfd = clientfd;
        clients[i]->status = F_CONNECTING;
        FD_SET(clientfd, &rfds_a);
        FD_SET(clientfd, &wfds_a);
    }
    
    for (;;) {
        /* restore previous selected fds */
        memcpy(&rfds, &rfds_a, sizeof(rfds));
        memcpy(&wfds, &wfds_a, sizeof(wfds));
        
        Select(nfds, &rfds, &wfds, NULL, NULL);
        
        for (int i = 0; i < MAX_CLIENT; i++) {
            Client *c = clients[i];

            if (c == NULL)
                continue;
            
            if (c->status == F_CONNECTING &&
                (FD_ISSET(c->sockfd, &rfds) || FD_ISSET(c->sockfd, &wfds))) {


                int error;
                socklen_t clilen = sizeof(struct sockaddr_in);
                if (getsockopt(c->sockfd, SOL_SOCKET, SO_ERROR, &error, &clilen) < 0 ||
                    error != 0) {
                    fprintf(stderr, "non-blocking connect failed\n");
                    exit(1);
                }
                c->status = F_READING;
                FD_CLR(c->sockfd, &wfds);
            } else if (c->status == F_READING && FD_ISSET(c->sockfd, &rfds) ) {
                char buf[1024];
                Read(c->sockfd, buf, sizeof(buf));
                if (i == 0)
                    printf("===%d===\n%s", i, buf);
                FD_CLR(c->sockfd, &rfds);
                FD_SET(c->sockfd, &wfds);
                c->status = F_WRITING;
            } else if (c->status == F_WRITING && FD_ISSET(c->sockfd, &wfds)) {
                if ( i == 0) {
                    printf("printenv path\n");
                }
                Write(c->sockfd, "printenv path\n", 14);
                c->status = F_DONE;
                FD_CLR(c->sockfd, &wfds);
                FD_CLR(c->sockfd, &rfds);
            }
        }
    }
    return 0;
}


int new_client_fd(char *hostname, ushort port) {
    struct sockaddr_in servaddr;
    
    int clientfd = Socket(AF_INET, SOCK_STREAM, 0);
    
    struct hostent *he = gethostbyname(hostname);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr = *((struct in_addr *)he->h_addr);
    servaddr.sin_port = htons(port);

    // Set non-blocking
    int flags = fcntl(clientfd, F_GETFL, 0);
    fcntl(clientfd, F_SETFL, flags | O_NONBLOCK);

    if(connect(clientfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1) {
        if (errno != EINPROGRESS)
            err_sys("Connect failed");
    }
    return clientfd;
}

Client** parse_query_string(char *querystring) {
    char* item = strtok(querystring, "&");
    
    Client* *clients = malloc(sizeof(void*) * MAX_CLIENT);
    bzero(clients, sizeof(*clients) * MAX_CLIENT);
    
    if (item == NULL)
        return NULL;
    
    while (item != NULL) {
        char* ptr;
        char *key = strtok_r(item, "=", &ptr);

        if (key == NULL || strlen(key) != 2)
            break;
        
        char *val = strtok_r(NULL, "", &ptr);
        
        if (val == NULL)
            break;
        
        int clientid = atoi(key + 1);
        
        if (clientid == 0 || clientid > 5)
            break;
        
        int i = clientid - 1;
        if (clients[i] == NULL) {
            clients[i] = malloc(sizeof(Client));
            bzero(clients[i], sizeof(Client));
            clients[i]->index = i;
        }

        switch (*key) {
            case 'h':
                clients[i]->host = strndup(val, 100);
                break;
            case 'p':
                clients[i]->port = atoi(val);
                break;
            case 'f':
                clients[i]->batch = fopen(val, "r");
                break;
            default:
                break;
        }
        printf("%s=%s\n", key, val);
        item = strtok(NULL, "&");
    }
    return clients;
}

void print_html_frame(Client* *clients) {
    printf("<html>\n");
    printf("<head>\n");
    printf("	<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\n");
    printf("	<title>Network Programming Homework 3</title>\n");
    printf("</head>\n");
    printf("<body bgcolor=#336699>\n");
    printf("	<font face=\"Courier New\" size=2 color=#FFFF99>\n");
    printf("		<table width=\"800\" border=\"1\">\n");
    printf("			<tr>\n");
    
    for (int i = 0; i < MAX_CLIENT; i++) {
        Client* client = clients[i];
        if (client == NULL)
            continue;
        printf("		  <td>%s</td>\n", client->host);
    }
    printf("			</tr>\n");
    printf("			<tr>\n");

    for (int i = 0; i < MAX_CLIENT; i++) {
        Client* client = clients[i];
        if (client == NULL)
            continue;
        printf("			<td valign=\"top\" id=\"m%d\"></td>\n", i);
    }
    printf("			</tr>\n");
    printf("		</table>\n");
    printf("	</font>\n");
    printf("</body>\n");
    printf("</html>\n");
}

void printc(Client* client, char* format, ...) {
    va_list args;

    printf("<script>document.all['m%d'].innerHTML += \"", client->index);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\";</script>");
}