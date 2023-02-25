/**
 * @file    common/tcp.h
 * @brief   这个文件声明tcp连接工具函数
 * @date    2023-02-23
 */


#ifndef TCP_H
#define TCP_H

#include <arpa/inet.h>


/**
 * @brief   tcp客户端连接，成功返回sockfd，不成功返回-1
 * 
 * @param address 
 * @param port 
 * @return int 
 */
int tcp_client_conn_n(in_addr_t sin_addr, int port);

/**
 * @brief   tcpd客户端连接，成功返回sockfd，不成功返回-1
 * 
 * @param address 
 * @param port 
 * @return int 
 */
int tcp_client_conn_a(char *address, int port);


/**
 * @brief   tcp服务端连接，成功返回listenfd，不成功返回-1
 * 
 * @param port 
 * @return int 
 */
int tcp_server_listen(int port);


/**
 * @brief   tcp服务端连接，成功返回sockfd，不成功返回-1
 * 
 * @param port 
 * @return * int 
 */
int tcp_server_conn(int port);
#endif