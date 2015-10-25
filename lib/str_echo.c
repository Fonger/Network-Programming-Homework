#include	"unp.h"
#include <stdio.h>

void showSymbol(int sockfd);

void
str_echo(int sockfd)
{
    ssize_t		n;
    char		buf[MAXLINE];

again:
    showSymbol(sockfd);
    while ( (n = read(sockfd, buf, MAXLINE)) > 0) {
        Writen(sockfd, buf, n);
        showSymbol(sockfd);
    }
    if (n < 0 && errno == EINTR)
        goto again;
    else if (n < 0)
        err_sys("str_echo: read error");
}

void showSymbol(int sockfd) {
    char *symbol = "% ";
    Writen(sockfd, symbol, 2);
}