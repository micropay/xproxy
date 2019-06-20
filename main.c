#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/wait.h>
#include <pthread.h>

#include "comm.h"
#include "wrap.h"

#define MAXLINE 4096
#define SERV_PORT 1080
#define OPEN_MAX 5000
//#define REMOTE_SERV_ADDR "www.wenming.cn"
//#define REMOTE_SERV_ADDR "47.92.167.38"
#define REMOTE_SERV_PORT 80

ssize_t efd; //红黑树根节点

// 处理僵尸进程
/*void sigchld_handler(int signal) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}*/

#if 0
int get_line(int sock, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n;

	while ((i < size - 1) && (c != '\n'))
	{
		n = recv(sock, &c, 1, 0);
		/* DEBUG printf("%02X\n", c); */
		if (n > 0)
		{
			if (c == '\r')
			{
				n = recv(sock, &c, 1, MSG_PEEK);
				/* DEBUG printf("%02X\n", c); */
				if ((n > 0) && (c == '\n'))
					recv(sock, &c, 1, 0);
				else
					c = '\n';
			}
			buf[i] = c;
			i++;
		}
		else
			c = '\n';
	}
	buf[i] = '\0';

	return(i);
}
#endif

// 处理客户端请求
void handle_client(void *arg){
    char str[INET_ADDRSTRLEN] = {0};
    char buf[MAXLINE];
    char path[255] = {0};
    int remote_srv_fd, n, res, sockfd;
    struct hostent *hp;
    char **pptr;
    struct sockaddr_in remote_servaddr;

    sockfd = *(int *)arg;
    n = Read(sockfd, buf, MAXLINE);

    if(n == 0){
        res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL);
        if(res == -1)
            DEBUG_LOG("epoll_ctl error, errno=%d, efd=%d, sockfd=%d\n", errno, (int)efd, (int)sockfd);

        Close(sockfd);
        printf("client[%d] closed connection\n", sockfd);
    }else if(n < 0){
        DEBUG_LOG("read n < 0 error, errno=%d\n", errno);
        res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL);
        Close(sockfd);
    }else{
        //解析服务器地址信息
        sscanf(buf, "%*s http://%[^ /]", path);
        hp = gethostbyname(path);

        if(!hp){
            DEBUG_LOG("gethostbyname error, path=%s\nbuf=%s\n",path,buf);
            Close(sockfd);
            pthread_exit((void *)1);
        }

        pptr = hp->h_addr_list;
        inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str));
        printf("address:%s\n", str);

        /*for(; *pptr!=NULL; pptr++)
        printf(" address:%s\n", inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str)));*/

        remote_srv_fd = Socket(AF_INET, SOCK_STREAM, 0);
        bzero(&remote_servaddr, sizeof(remote_servaddr));
        remote_servaddr.sin_family = AF_INET;
        inet_pton(AF_INET, str, &remote_servaddr.sin_addr);
        remote_servaddr.sin_port = htons(REMOTE_SERV_PORT);

        Connect(remote_srv_fd, (struct sockaddr *)&remote_servaddr, sizeof(remote_servaddr));

        n = Write(remote_srv_fd, buf, n);
        while(1){
            n = read(remote_srv_fd, buf, MAXLINE);
            if(n < 0){
                if(errno == EINTR){
                    continue;
                }else{
                    break;
                }
            }else if(n == 0){
                break;
            }
            
            Write(sockfd, buf, n);
        }

        Close(sockfd);
        Close(remote_srv_fd);
    }
}

int main(int argc, char *argv[])
{
    int i, listenfd, connfd, sockfd;
    int num = 0;
    //char **pptr;
    
    //pid_t pid;
    pthread_t tid;
    ssize_t nready, res;
    char str[INET_ADDRSTRLEN] = {0};
    socklen_t clilen;

    struct sockaddr_in cliaddr, servaddr;
    struct epoll_event tep, ep[OPEN_MAX];
    //struct hostent *hp;

    //signal(SIGCHLD, sigchld_handler); // 防止子进程变成僵尸进程

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    Listen(listenfd, 20);

    //创建epoll
    efd = epoll_create(OPEN_MAX);
    if(efd == -1)
        DEBUG_LOG("epoll_create error, errno %d", errno);

    tep.events = EPOLLIN;
    tep.data.fd = listenfd;
    res = epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &tep);
    if(res == -1)
        DEBUG_LOG("epoll_ctl error");

    printf("waiting for connect ......\n");
    while(1){
        nready = epoll_wait(efd, ep, OPEN_MAX, -1);
        if(nready == -1)
            DEBUG_LOG("epoll_wait error");

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
                    DEBUG_LOG("epoll_ctl error");
            }else{
                sockfd = ep[i].data.fd;
                //多线程处理
                pthread_create(&tid, NULL, (void *)&handle_client, (void *)&sockfd);
                pthread_detach(tid);         //线程分离
                // 多线程处理
                /*pid = fork();       
                if (pid == -1) {
                    DEBUG_LOG("fork err");
                    exit(0);
                }else if(pid == 0){
                    //处理请求
                    handle_client(sockfd);
                }*/
            }
        }
    }

    Close(listenfd);
    Close(efd);
    return 0;
}