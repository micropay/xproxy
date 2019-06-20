#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>

#include "wrap.h"

#define MAXLINE 8192
#define SERV_PORT 8000
#define OPEN_MAX 5000
#define REMOTE_SERV_ADDR "www.baidu.com"
#define REMOTE_SERV_PORT 80

int main(int argc, char *argv[])
{
    int i, listenfd, connfd, sockfd, remote_srv_fd;
    int n, num = 0;
    char **pptr;
    ssize_t nready, efd, res;
    char buf[MAXLINE], str[INET_ADDRSTRLEN];
    socklen_t clilen;

    struct sockaddr_in cliaddr, servaddr, remote_servaddr;
    struct epoll_event tep, ep[OPEN_MAX];
    struct hostent *hp;

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    Listen(listenfd, 20);

    //远程服务器建立连接
    hp = gethostbyname(REMOTE_SERV_ADDR);
    printf("webserver: %s\n", REMOTE_SERV_ADDR);

    pptr = hp->h_addr_list;
    inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str));
    printf("address:%s\n", str);

    /*for(; *pptr!=NULL; pptr++)
        printf(" address:%s\n", inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str)));*/

    //strcpy(remote_ip, );
    remote_srv_fd = Socket(AF_INET, SOCK_STREAM, 0);
    bzero(&remote_servaddr, sizeof(remote_servaddr));
    remote_servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, str, &remote_servaddr.sin_addr);
    remote_servaddr.sin_port = htons(REMOTE_SERV_PORT);

    Connect(remote_srv_fd, (struct sockaddr *)&remote_servaddr, sizeof(remote_servaddr));

    //创建epoll
    efd = epoll_create(OPEN_MAX);
    if(efd == -1)
        perr_exit("epoll_create error");

    tep.events = EPOLLIN;
    tep.data.fd = listenfd;
    res = epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &tep);
    if(res == -1)
        perr_exit("epoll_ctl error");

    printf("waiting for connect ......\n");
    while(1){
        nready = epoll_wait(efd, ep, OPEN_MAX, -1);
        if(nready == -1)
            perr_exit("epoll_wait error");

        for(i = 0; i < nready; i++){
            if(!(ep[i].events & EPOLLIN))
                continue;
            
            if(ep[i].data.fd == listenfd){
                clilen = sizeof(cliaddr);
                connfd = Accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

                printf("received from %s at port %d\n",
                    inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
                    ntohs(cliaddr.sin_port));
                printf("cfd %d ---- client %d\n", connfd, ++num);

                tep.events = EPOLLIN;
                tep.data.fd = connfd;
                res = epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &tep);
                if(res == -1)
                    perr_exit("epoll_ctl error");
            }else{
                sockfd = ep[i].data.fd;
                n = Read(sockfd, buf, MAXLINE);

                if(n == 0){
                    res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL);
                    if(res == -1)
                        perr_exit("epoll_ctl error");

                    Close(sockfd);
                    printf("client[%d] closed connection\n", sockfd);
                }else if(n < 0){
                    perr_exit("read n < 0 error: ");
                    res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL);
                    Close(sockfd);
                }else{
                    //for(i = 0; i < n; i++)
                    //    buf[i] = buf[i];
                        //buf[i] = toupper(buf[i]);
                    Write(remote_srv_fd, buf, n);
                    printf("1.proxy send len=%d\n%s\n", n, buf);
                    while(1){
                        n = read(remote_srv_fd, buf, MAXLINE);
                        if(errno == EINTR){
                            continue;
                        }
                        if(n == 0)
                            break;
                        printf("====\n%s", buf);
                        //Writen(sockfd, buf, n);
                    }
                    /*n = Read(remote_srv_fd, buf, MAXLINE);
                    printf("2.proxy recv len=%d\n%s\n", n, buf);
                    //Write(STDOUT_FILENO, buf, n);
                    Writen(sockfd, buf, n);
                    printf("3.proxy return len=%d\n%s\n", n, buf);*/
                }
            }
        }
    }

    Close(listenfd);
    Close(efd);
    return 0;
}