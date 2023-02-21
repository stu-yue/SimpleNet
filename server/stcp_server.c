/**
 * @file stcp_server.c
 * @brief 定义所有面向应用层的接口
 * @date 2023-02-19
 */


#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include "stcp_server.h"
#include "../common/constants.h"



server_tcb_t* tcbTable[MAX_TRANSPORT_CONNECTIONS];	// server的TCB表
int sip_conn;	// SIP进程的TCP连接
pthread_mutex_t tcbTable_mutex;


//通过给定的套接字获得tcb.
//如果没有找到tcb, 返回NULL
server_tcb_t* getTcb(int sockfd) 
{
	if (tcbTable[sockfd] != NULL)
		return tcbTable[sockfd];
	else
		return NULL;
}

// 通过给定的服务端端口号获得tcb. 
// 如果没有找到tcb, 返回NULL. 
server_tcb_t* getTcbFromPort(unsigned int serverPort)
{
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		if(tcbTable[i] != NULL && tcbTable[i]->server_portNum == serverPort) {
			return tcbTable[i];
		}
	}
	return NULL;
}

// 查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 返回新的tcb的索引号, 如果tcbtable中所有的tcb都已使用了或给定的端口号已被使用, 就返回-1.
int newTcb(unsigned int port) 
{
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		if(tcbTable[i] != NULL && tcbTable[i]->server_portNum == port) {
			return -1;
		}
	}
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		if(tcbTable[i] == NULL) {
			tcbTable[i] = (server_tcb_t*) malloc(sizeof(server_tcb_t));
			tcbTable[i]->server_portNum = port;
			return i;
		}
	}
	return -1;
}

void stcp_server_init(int conn) 
{
	// 初始化全局变量
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		tcbTable[i] = NULL;
	}
	sip_conn = conn;
	pthread_mutex_init(&tcbTable_mutex, NULL);

	// 启动seghandler
	pthread_t seghandler_thread;
	pthread_create(&seghandler_thread, NULL, seghandler, NULL);
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port) 
{
	// 获得一个新的tcb
	int sockfd = newTcb(server_port);
	if (sockfd == -1)
		return -1;
	
	server_tcb_t* tcb = getTcb(sockfd);

	// 初始化tcb
	pthread_mutex_lock(&tcbTable_mutex);
	tcb->server_nodeID = -1;
	tcb->client_nodeID = -1;
	tcb->state = CLOSED;
	// 为接收缓冲区动态分配
	char* recvBuf;
	recvBuf = (char*) malloc(sizeof(char) * RECEIVE_BUF_SIZE);
	assert(recvBuf != NULL);
	tcb->recvBuf = recvBuf;
	tcb->usedBufLen = 0;
	// 为接收缓冲区创建互斥量
	pthread_mutex_t* sendBuf_mutex;
	sendBuf_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	assert(sendBuf_mutex != NULL);
	pthread_mutex_init(sendBuf_mutex, NULL);
	tcb->bufMutex = sendBuf_mutex;
	pthread_mutex_unlock(&tcbTable_mutex);

	return sockfd;
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd) 
{
	// 通过sockfd获得tcb
	server_tcb_t* serverTcb = getTcb(sockfd);
	if (!serverTcb)
		return -1;

	switch (serverTcb->state) {
		case CLOSED:
			pthread_mutex_lock(&tcbTable_mutex);
			// 状态更新
			serverTcb->state = LISTENING;
			printf("SERVER: LISTENING\n");
			while (1) {
				pthread_mutex_unlock(&tcbTable_mutex);
				select(0, 0, 0, 0, &(struct timeval){.tv_usec = ACCEPT_POLLING_INTERVAL / 1000});
				pthread_mutex_lock(&tcbTable_mutex);
				if (serverTcb->state == CONNECTED) {
					pthread_mutex_unlock(&tcbTable_mutex);
					return 1;
				}
			}
		case LISTENING:
			return -1;
		case CONNECTED:
			return -1;
		case CLOSEWAIT:
			return -1;
		default:
			return -1;
	}
}

// 接收来自STCP客户端的数据
//
// 这个函数接收来自STCP客户端的数据. 你不需要在本实验中实现它.
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length) 
{
	// 通过sockfd获得tcb
	server_tcb_t* serverTcb = getTcb(sockfd);
  	if (!serverTcb) 
    	return -1;

	unsigned int len = length;
	
	switch (serverTcb->state) {
		case CLOSED:
			return -1;
		case LISTENING:
			return -1;
		case CONNECTED:
			// printf("len: %d\n", length);
			while (1) {
				pthread_mutex_lock(serverTcb->bufMutex);
				// printf("len: %d\tlength: %d\tusedLen: %d\n", len, length, serverTcb->usedBufLen);
				if (length <= serverTcb->usedBufLen) {
					memcpy(buf, serverTcb->recvBuf, length);
					serverTcb->usedBufLen -= length;
					len += length;
					memcpy(serverTcb->recvBuf, serverTcb->recvBuf + length, serverTcb->usedBufLen);
					//printf("buf: %s\tuseLen: %d\n", serverTcb->recvBuf, serverTcb->usedBufLen);
					pthread_mutex_unlock(serverTcb->bufMutex);
					// printf("\n\nlen: %d\tlength: %d\tusedLen: %d\n\n", len, length, serverTcb->usedBufLen);
					// if (length > 4) assert(0);
					return 1;
				}
				pthread_mutex_unlock(serverTcb->bufMutex);
				select(0, 0, 0, 0, &(struct timeval){.tv_sec = RECVBUF_POLLING_INTERVAL});
			}
		case CLOSEWAIT:
			return -1;
		default:
			return -1;
	}
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd) 
{
	// 通过sockfd获得tcb
	server_tcb_t* serverTcb = getTcb(sockfd);
  	if (!serverTcb) 
    	return -1;
	
  	switch (serverTcb->state) {
    	case CLOSED:
			free(serverTcb->recvBuf);
			free(serverTcb->bufMutex);
      		free(tcbTable[sockfd]);
      		tcbTable[sockfd] = NULL;
      		return 1;
    	case LISTENING:
      		return -1;
    	case CONNECTED:
      		return -1;
    	case CLOSEWAIT:
      		return -1;
    	default:
      		return -1;
  	}
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//

void *seghandler(void* arg) {
	seg_t segBuf;
	while (1) {
		// 客户端连接关闭
		int n;
		if ((n = sip_recvseg(sip_conn, &segBuf)) < 0) {
			close(sip_conn);
			pthread_exit(NULL);
		}
		if (n == 0) continue;

		// 找到tcb来处理
		server_tcb_t* serverTcb = getTcbFromPort(segBuf.header.dest_port);
		if (!serverTcb) {
			printf("SERVER: NO PORT FOR RECEIVED SEGMENTs\n");
			continue;
		}

		// 段处理
		pthread_mutex_lock(&tcbTable_mutex);
		switch (serverTcb->state) {
			case CLOSED:
				break;
			case LISTENING:
				if (segBuf.header.type == SYN) {
					// 状态转换
					serverTcb->state = CONNECTED;
					serverTcb->client_portNum = segBuf.header.src_port;
					// 用接收到的 SYN 段中的序号来设置 exepct_seqNum
					serverTcb->expect_seqNum = segBuf.header.seq_num;
					printf("SERVER: CONNECTED (RECEIVED SYN)\n");
					seg_t synack;
					synack.header.type = SYNACK;
					synack.header.src_port = serverTcb->server_portNum;
					synack.header.dest_port = serverTcb->client_portNum;
					synack.header.seq_num = 0;
					synack.header.ack_num = serverTcb->expect_seqNum;
					synack.header.length = 0;
					sip_sendseg(sip_conn, &synack);
				}
				else
					printf("SERVER: IN LISTENING, NO SYN SEG RECEIVED\n");
				break;
			case CONNECTED:
				if (segBuf.header.type == SYN && segBuf.header.src_port == serverTcb->client_portNum) {
					printf("SERVER: REVEIVED SYN (HAS CONNECTED)\n");
					seg_t synack;
					synack.header.type = SYNACK;
					synack.header.src_port = serverTcb->server_portNum;
					synack.header.dest_port = serverTcb->client_portNum;
					synack.header.seq_num = 0;
					synack.header.ack_num = serverTcb->expect_seqNum;
					synack.header.length = 0;
					sip_sendseg(sip_conn, &synack);
				} else if (segBuf.header.type == DATA && segBuf.header.src_port == serverTcb->client_portNum) {
					printf("SERVER: RECEIVED DATA [SEQ: %3d | EXP: %3d] ", segBuf.header.seq_num, serverTcb->expect_seqNum);
					if (segBuf.header.seq_num == serverTcb->expect_seqNum) {
						pthread_mutex_lock(serverTcb->bufMutex);
						if (serverTcb->usedBufLen + segBuf.header.length <= RECEIVE_BUF_SIZE) {
							memcpy(serverTcb->recvBuf + serverTcb->usedBufLen, segBuf.data, segBuf.header.length);
							serverTcb->usedBufLen += segBuf.header.length;
							serverTcb->expect_seqNum += segBuf.header.length;
							printf("[LEN: %3d | USED: %3d]\n", segBuf.header.length, serverTcb->usedBufLen);
						}
						else 
							printf("[BUF IS FULL -> DISCARD]\n");
						pthread_mutex_unlock(serverTcb->bufMutex);
					} else {
						printf("[SEQ != EXP -> DISCARD]\n");
					}
					seg_t dataack;
					dataack.header.type = DATAACK;
					dataack.header.src_port = serverTcb->server_portNum;
					dataack.header.dest_port = serverTcb->client_portNum;
					dataack.header.seq_num = 0;
					dataack.header.ack_num = serverTcb->expect_seqNum;
					dataack.header.length = 0;
					sip_sendseg(sip_conn, &dataack);
				} else if (segBuf.header.type == FIN && segBuf.header.src_port == serverTcb->client_portNum) {
					// 状态更新
					serverTcb->state = CLOSEWAIT;
					printf("SERVER: CLOSEWAIT (RECEIVED FIN)\n");
					seg_t finack;
					finack.header.type = FINACK;
					finack.header.src_port = serverTcb->server_portNum;
					finack.header.dest_port = serverTcb->client_portNum;
					finack.header.seq_num = 0;
					finack.header.ack_num = serverTcb->expect_seqNum;
					finack.header.length = 0;
					sip_sendseg(sip_conn, &finack);
					
					// 启动定时器，CLOSEWAIT_TIMEOUT后关闭
					pthread_t timer;
					pthread_create(&timer, NULL, closewait_timer, (void*)serverTcb);
				}
				break;
			case CLOSEWAIT:
				if (segBuf.header.type == FIN && segBuf.header.src_port == serverTcb->client_portNum) {
					printf("SERVER: RECEIVED FIN (HAS CLOSEWAIT)\n");
					seg_t finack;
					finack.header.type = FINACK;
					finack.header.src_port = serverTcb->server_portNum;
					finack.header.dest_port = serverTcb->client_portNum;
					finack.header.seq_num = 0;
					finack.header.ack_num = serverTcb->expect_seqNum;
					finack.header.length = 0;
					sip_sendseg(sip_conn, &finack);
				}
				else
					printf("SERVER: IN CLOSEWAIT, NO FIN SEG RECEIVED\n");
				break;
		}
		pthread_mutex_unlock(&tcbTable_mutex);
	}
}


// CLOSEWAIT定时器函数
void *closewait_timer(void* arg) 
{
	server_tcb_t* serverTcb = (server_tcb_t*) arg;
	struct timeval initTime, currentTime;
	gettimeofday(&initTime, NULL);
	while (1) {
		gettimeofday(&currentTime, NULL);
		double diff = (currentTime.tv_sec - initTime.tv_sec) + 1e-6 * (currentTime.tv_usec - initTime.tv_usec);
		if (diff >= CLOSEWAIT_TIMEOUT) {
			pthread_mutex_lock(&tcbTable_mutex);
			// 状态更新
			serverTcb->state = CLOSED;
			printf("SERVER: CLOSED (CLOSEWAIT TIMEOUT)\n");
			serverTcb->client_nodeID = -1;
			serverTcb->client_portNum = 0;
			serverTcb->expect_seqNum = 0;
			pthread_mutex_unlock(&tcbTable_mutex);
			pthread_exit(NULL);
		}
	}
}
