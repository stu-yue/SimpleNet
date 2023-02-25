/**
 * @file    common/tcp.c
 * @brief   这个文件实现tcp连接工具函数
 * @date    2023-02-23
 */


#include "tcp.h"
#include "stdio.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


int tcp_client_conn_n(in_addr_t address, int port) {
	int socket_fd;
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = address;

    socklen_t server_len = sizeof(server_addr);
    int connect_rt = connect(socket_fd, (struct sockaddr *) &server_addr, server_len);
    if (connect_rt < 0) {
        printf("Client: connect failed\n");
		return -1;
    }
    return socket_fd;
}


int tcp_client_conn_a(char *address, int port) {
    in_addr_t sin_addr;
    inet_pton(AF_INET, address, &sin_addr);

    return tcp_client_conn_n(sin_addr, port);
}


int tcp_server_listen(int port) {
    int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    int rt1 = bind(listenfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (rt1 < 0) {
    	printf("Server: bind failed\n");
		return -1;
    }

    int rt2 = listen(listenfd, 1024);
    if (rt2 < 0) {
    	printf("Server: listen failed\n");
		return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    return listenfd;
}


int tcp_server_conn(int port) {
    int listenfd;
    listenfd = tcp_server_listen(port);

    int connfd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    if ((connfd = accept(listenfd, (struct sockaddr *) &client_addr, &client_len)) < 0) {
        printf("Server: accept failed\n");
		return -1;
    }

    return connfd;
}