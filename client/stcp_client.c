/**
 * @file stcp_client.c
 * @brief 定义所有面向应用层的接口
 * @date 2023-02-19
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include "stcp_client.h"


client_tcb_t* tcbTable[MAX_TRANSPORT_CONNECTIONS];  // client的TCB表
int sip_conn;	// SIP进程的TCP连接
pthread_mutex_t tcbTable_mutex;


//通过给定的套接字获得tcb.
//如果没有找到tcb, 返回NULL.
client_tcb_t* getTcb(int sockfd) 
{
	if (tcbTable[sockfd] != NULL)
		return tcbTable[sockfd];
  	else
    	return NULL;
}

//通过给定的客户端端口号获得tcb. 
//如果没有找到tcb, 返回NULL. 
client_tcb_t* getTcbFromPort(unsigned int clientPort)
{
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		if(tcbTable[i] != NULL && tcbTable[i]->client_portNum == clientPort) {
			return tcbTable[i];
		}
	}
	return NULL;
}

//从tcbTable中获得一个新的tcb, 使用给定的端口号分配给客户端端口.
//返回新的tcb的索引号, 如果tcbtable中所有的tcb都已使用了或给定的端口号已被使用, 就返回-1.
int newTcb(unsigned int port) 
{
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		if(tcbTable[i] != NULL && tcbTable[i]->client_portNum == port) {
			return -1;
		}
	}
	
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		if(tcbTable[i] == NULL) {
			tcbTable[i] = (client_tcb_t*) malloc(sizeof(client_tcb_t));
			tcbTable[i]->client_portNum = port;
			return i;
		}
	}
	return -1;
}


void stcp_client_init(int conn) 
{
	// 初始化全局变量
	for (int  i = 0; i < MAX_TRANSPORT_CONNECTIONS; i++) {
		tcbTable[i] = NULL;
	}
	sip_conn = conn;
	pthread_mutex_init(&tcbTable_mutex, NULL);

	// 启动seghandler
	pthread_t seghandler_thread;
	pthread_create(&seghandler_thread, NULL, seghandler, NULL);
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) 
{
	// 获得一个新的tcb
	int sockfd = newTcb(client_port);
	if (sockfd < 0)
		return -1;

	client_tcb_t* tcb = getTcb(sockfd);

	// 初始化tcb
	pthread_mutex_lock(&tcbTable_mutex);
	tcb->server_nodeID = -1;
	tcb->client_nodeID = -1;
	tcb->state = CLOSED;
	tcb->next_seqNum = 0;
	tcb->sendBufHead = NULL;
	tcb->sendBufunSent = NULL;
	tcb->sendBufTail = NULL;
	tcb->unAck_segNum = 0;
	// 为发送缓冲区创建互斥量
	pthread_mutex_t* sendBuf_mutex;
	sendBuf_mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	assert(sendBuf_mutex != NULL);
	pthread_mutex_init(sendBuf_mutex, NULL);
	tcb->bufMutex = sendBuf_mutex;
	pthread_mutex_unlock(&tcbTable_mutex);

	return sockfd;

}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, unsigned int server_port) 
{
	// 通过sockfd获得tcb
	client_tcb_t* clientTcb = getTcb(sockfd);
	if (!clientTcb)
		return -1;

	switch (clientTcb->state) {
		case CLOSED:
			pthread_mutex_lock(&tcbTable_mutex);
			// 绑定服务器端 nodeID 和 port
			clientTcb->server_portNum = server_port;
			// 发送 SYN 段报文
			seg_t syn;
			bzero(&syn, sizeof(syn));
			syn.header.type = SYN;
			syn.header.src_port = clientTcb->client_portNum;
			syn.header.dest_port = clientTcb->server_portNum;
			syn.header.seq_num = 0;
			syn.header.length = 0;
			sip_sendseg(sip_conn, &syn);
			
			//状态转换
			clientTcb->state = SYNSENT;
			printf("CLIENT: SYN SENT\n");

			// 设置计时器，超时重传
			int retry = SYN_MAX_RETRY;
			while (retry > 0) {
				pthread_mutex_unlock(&tcbTable_mutex);
				select(0, 0, 0, 0, &(struct timeval){.tv_usec = SYN_TIMEOUT / 1000});
				pthread_mutex_lock(&tcbTable_mutex);
				if (clientTcb->state == CONNECTED) {
					pthread_mutex_unlock(&tcbTable_mutex);
					return 1;
				} else {
					printf("CLIENT: SYN RESENT (%d/%d)\n", SYN_MAX_RETRY - retry + 1, SYN_MAX_RETRY);
					sip_sendseg(sip_conn, &syn);
					retry--;
				}
			}
			// 状态转换
			clientTcb->state = CLOSED;
			printf("CLIENT: CLOSED\n");
			pthread_mutex_unlock(&tcbTable_mutex);
			return -1;
		case SYNSENT:
			return -1;
		case CONNECTED:
			return -1;
		case FINWAIT:
			return -1;
		default:
			return -1;
	}
}


int stcp_client_send(int sockfd, void* data, unsigned int length) 
{
	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) 
{
	//通过sockfd获得tcb
	client_tcb_t* clientTcb = getTcb(sockfd);
	if (!clientTcb)
		return -1;
	
	seg_t fin;
	switch (clientTcb->state) {
		case CLOSED:
			return -1;
		case SYNSENT:
			return -1;
		case CONNECTED:
			pthread_mutex_lock(&tcbTable_mutex);
			// 发送 FIN 段报文
			bzero(&fin, sizeof(fin));
			fin.header.type = FIN;
			fin.header.src_port = clientTcb->client_portNum;
			fin.header.dest_port = clientTcb->server_portNum;
			fin.header.length = 0;
			sip_sendseg(sip_conn, &fin);
			printf("CLIENT: FIN SENT\n");
			// 更新状态
			clientTcb->state = FINWAIT;
			printf("CLIENT: FINWAIT\n");

			// 设置计时器，超时重传
			int retry = FIN_MAX_RETRY;
			while (retry > 0) {
				pthread_mutex_unlock(&tcbTable_mutex);
				select(0, 0, 0, 0, &(struct timeval){.tv_usec = FIN_TIMEOUT / 1000});
				pthread_mutex_lock(&tcbTable_mutex);
				if (clientTcb->state == CLOSED) {
					clientTcb->server_nodeID = -1;
					clientTcb->server_portNum = 0;
					clientTcb->next_seqNum = 0;
					pthread_mutex_unlock(&tcbTable_mutex);
					return 1;
				}
				else {
					printf("CLIENT: FIN RESENT (%d/%d)\n", FIN_MAX_RETRY - retry + 1, FIN_MAX_RETRY);
					sip_sendseg(sip_conn, &fin);
					retry--;
				}
			}
			// 发送失败，仍关闭
			clientTcb->state = CLOSED;
			printf("CLIENT: CLOSED\n");
			pthread_mutex_unlock(&tcbTable_mutex);
			return -1;
		case FINWAIT:
			return -1;
		default:
			return -1;
	}
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) 
{
	// 通过sockfd获得tcb
	client_tcb_t* clientTcb = getTcb(sockfd);
	if (!clientTcb) 
		return -1;
	
	switch (clientTcb->state) {
		case CLOSED:
			free(clientTcb->bufMutex);
			free(tcbTable[sockfd]);
			tcbTable[sockfd] = NULL;
			return 1;
		case SYNSENT:
			return -1;
		case CONNECTED:
			return -1;
		case FINWAIT:
			return -1;
		default:
			return -1;
	}
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg) 
{
	seg_t segBuf;
	int src_nodeID;
	while (1) {
		// 接收到一个段
		int n;
		if ((n = sip_recvseg(sip_conn, &segBuf)) < 0) {
			close(sip_conn);
			pthread_exit(NULL);
		}
		if (n == 0) continue;

		// 找到tcb来处理
		client_tcb_t* clientTcb = getTcbFromPort(segBuf.header.dest_port);
		if (!clientTcb) {
			printf("CLIENT: NO PORT FOR RECEIVED SEGMENT\n");
			continue;
		}

		// 段处理
		pthread_mutex_lock(&tcbTable_mutex);
		switch (clientTcb->state) {
			case CLOSED:
				break;
			case SYNSENT:
				if (segBuf.header.type == SYNACK && segBuf.header.src_port == clientTcb->server_portNum) {
					clientTcb->state = CONNECTED;
					printf("CLIENT: CONNECTED (RECEIVED SYNACK)\n");
				}
				else
					printf("CLIENT: IN SYNSENT, NO SYNACK SEG RECEIVED\n");
				break;
			case CONNECTED:
				if (segBuf.header.type == DATAACK && segBuf.header.src_port == clientTcb->server_portNum) {
				
				}
				else
					printf("CLIENT: IN CONNECTED, NO DATAACK SEG RECEIVED\n");
				break;
			case FINWAIT:
				if (segBuf.header.type == FINACK && segBuf.header.src_port == clientTcb->server_portNum) {
					clientTcb->state = CLOSED;
					printf("CLIENT: CLOSED (RECEIVED FINACK)\n");
				}
				else
					printf("CLIENT: IN FINWAIT, NO FINACK SEG RECEIVED\n");
				break;
		}
		pthread_mutex_unlock(&tcbTable_mutex);
	}
}



