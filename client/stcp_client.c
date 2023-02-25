/**
 * @file 	stcp_client.c
 * @brief 	定义所有面向应用层的接口
 * @date 	2023-02-19
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
#include "../common/seg.h"
#include "../topology/topology.h"


client_tcb_t* tcbTable[MAX_TRANSPORT_CONNECTIONS];  // client的TCB表
int sip_conn;										// SIP进程的TCP连接
pthread_mutex_t tcbTable_mutex;


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
	tcb->client_nodeID = topology_getMyNodeID();
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


int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
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
			clientTcb->server_nodeID = nodeID;
			// 发送 SYN 段报文
			seg_t syn;
			bzero(&syn, sizeof(syn));
			syn.header.type = SYN;
			syn.header.src_port = clientTcb->client_portNum;
			syn.header.dest_port = clientTcb->server_portNum;
			syn.header.seq_num = 0;
			syn.header.length = 0;
			sip_sendseg(sip_conn, clientTcb->server_nodeID, &syn);
			
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
					sip_sendseg(sip_conn, clientTcb->server_nodeID, &syn);
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
	// 通过sockfd获得tcb
	client_tcb_t* clientTcb = getTcb(sockfd);
	if (!clientTcb)
		return -1;
	
	int segNum;
	switch (clientTcb->state) {
		case CLOSED:
			return -1;
		case SYNSENT:
			return -1;
		case CONNECTED:
			// 使用提供的数据创建段
			segNum = length / MAX_SEG_LEN;
			if (length % MAX_SEG_LEN)
			    segNum++;
			for (int i = 0; i < segNum; i++) {
				segBuf_t* newBuf = (segBuf_t*) malloc(sizeof(segBuf_t));
				assert(newBuf != NULL);
				bzero(newBuf, sizeof(segBuf_t));
				newBuf->seg.header.src_port = clientTcb->client_portNum;
				newBuf->seg.header.dest_port = clientTcb->server_portNum;
				if (length % MAX_SEG_LEN != 0 && i == segNum - 1)
					newBuf->seg.header.length = length % MAX_SEG_LEN;
				else
					newBuf->seg.header.length = MAX_SEG_LEN;
				newBuf->seg.header.type = DATA;
				char* datatosend = (char*)data;
				memcpy(newBuf->seg.data, &datatosend[i * MAX_SEG_LEN], newBuf->seg.header.length);
				sendBuf_addSeg(clientTcb, newBuf);
			}
			
			//发送段直到未确认段数目达到GBN_WINDOW为止, 如果sendBuf_timer没有启动, 就启动它.
			sendBuf_send(clientTcb);
			return 1;
		case FINWAIT:
			return -1;
		default:
			return -1;
	}
}


int stcp_client_disconnect(int sockfd) 
{
	// 通过sockfd获得tcb
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
			sip_sendseg(sip_conn, clientTcb->server_nodeID, &fin);
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
					sendBuf_clear(clientTcb);
					pthread_mutex_unlock(&tcbTable_mutex);
					return 1;
				}
				else {
					printf("CLIENT: FIN RESENT (%d/%d)\n", FIN_MAX_RETRY - retry + 1, FIN_MAX_RETRY);
					sip_sendseg(sip_conn, clientTcb->server_nodeID, &fin);
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


void *seghandler(void* arg) 
{
	seg_t segBuf;
	int src_nodeID;
	while (1) {
		// 接收到一个段
		int n;
		if ((n = sip_recvseg(sip_conn, &src_nodeID, &segBuf)) < 0) {
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
				if (segBuf.header.type == SYNACK 
						&& segBuf.header.src_port == clientTcb->server_portNum
						&& clientTcb->server_nodeID == src_nodeID) {
					clientTcb->state = CONNECTED;
					printf("CLIENT: CONNECTED (RECEIVED SYNACK)\n");
				}
				else
					printf("CLIENT: IN SYNSENT, NO SYNACK SEG RECEIVED\n");
				break;
			case CONNECTED:
				if (segBuf.header.type == DATAACK 
						&& segBuf.header.src_port == clientTcb->server_portNum
						&& clientTcb->server_nodeID == src_nodeID) {
					// 接收到ack, 更新发送缓冲区
					sendBuf_recvAck(clientTcb, segBuf.header.ack_num);
					// 发送在发送缓冲区中的新段
					sendBuf_send(clientTcb);
				}
				else
					printf("CLIENT: IN CONNECTED, NO DATAACK SEG RECEIVED\n");
				break;
			case FINWAIT:
				if (segBuf.header.type == FINACK 
						&& segBuf.header.src_port == clientTcb->server_portNum
						&& clientTcb->server_nodeID == src_nodeID) {
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


void* sendBuf_timer(void* clienttcb)
{
	client_tcb_t* clientTcb = (client_tcb_t*) clienttcb;
	while(1) {
		select(0, 0, 0, 0, &(struct timeval){.tv_usec = SENDBUF_POLLING_INTERVAL / 1000});
		
		struct timeval currentTime;
		gettimeofday(&currentTime, NULL);
		
		// 如果unAck_segNum为0, 意味着发送缓冲区中没有段, 退出.
		if (clientTcb->unAck_segNum == 0) {
			pthread_exit(NULL);
		}
		else if (clientTcb->sendBufHead->sentTime > 0 && clientTcb->sendBufHead->sentTime < currentTime.tv_sec * 1e6 + currentTime.tv_usec - DATA_TIMEOUT) {
			sendBuf_timeout(clientTcb);
		}
	}
}


void sendBuf_addSeg(client_tcb_t* clientTcb, segBuf_t* newSegBuf) 
{
	pthread_mutex_lock(clientTcb->bufMutex);
	if (clientTcb->sendBufHead == NULL) {
		newSegBuf->seg.header.seq_num = clientTcb->next_seqNum;
		clientTcb->next_seqNum += newSegBuf->seg.header.length;
		newSegBuf->sentTime = 0;
		clientTcb->sendBufHead = newSegBuf;
		clientTcb->sendBufunSent = newSegBuf;
		clientTcb->sendBufTail = newSegBuf;
	}
	else {
		newSegBuf->seg.header.seq_num = clientTcb->next_seqNum;
		clientTcb->next_seqNum += newSegBuf->seg.header.length;
		newSegBuf->sentTime = 0;
		clientTcb->sendBufTail->next = newSegBuf;
		clientTcb->sendBufTail = newSegBuf;
		if (clientTcb->sendBufunSent == NULL)
			clientTcb->sendBufunSent = newSegBuf;
	}
	pthread_mutex_unlock(clientTcb->bufMutex);
}


void sendBuf_send(client_tcb_t* clientTcb) 
{
	pthread_mutex_lock(clientTcb->bufMutex);

	while (clientTcb->unAck_segNum < GBN_WINDOW && clientTcb->sendBufunSent != NULL) {
		sip_sendseg(sip_conn, clientTcb->server_nodeID, (seg_t*)clientTcb->sendBufunSent);
		struct timeval currentTime;
		gettimeofday(&currentTime, NULL);
		clientTcb->sendBufunSent->sentTime = currentTime.tv_sec * 1e3 + currentTime.tv_usec;
		// 在发送了第一个数据段之后应启动sendBuf_timer
		if (clientTcb->unAck_segNum == 0) {
			pthread_t timer;
			pthread_create(&timer, NULL, sendBuf_timer, (void*)clientTcb);
		}
		clientTcb->unAck_segNum++;

		if (clientTcb->sendBufunSent != clientTcb->sendBufTail)
			clientTcb->sendBufunSent= clientTcb->sendBufunSent->next;
		else
			clientTcb->sendBufunSent = NULL; 
	}
	pthread_mutex_unlock(clientTcb->bufMutex);
}


void sendBuf_timeout(client_tcb_t* clientTcb) 
{
	pthread_mutex_lock(clientTcb->bufMutex);
	printf("CLIENT: SEND BUF TIMEOUT\n");
	segBuf_t* bufPtr = clientTcb->sendBufHead;
	for(int i = 0; i < clientTcb->unAck_segNum; i++) {
		sip_sendseg(sip_conn, clientTcb->server_nodeID, (seg_t*)bufPtr);
		struct timeval currentTime;
		gettimeofday(&currentTime, NULL);
		bufPtr->sentTime = currentTime.tv_sec * 1e6 + currentTime.tv_usec;
		bufPtr = bufPtr->next; 
	}
	pthread_mutex_unlock(clientTcb->bufMutex);
}


void sendBuf_recvAck(client_tcb_t* clientTcb, unsigned int ack_seqnum) 
{
	pthread_mutex_lock(clientTcb->bufMutex);

	//如果所有段都被确认了
	if (ack_seqnum > clientTcb->sendBufTail->seg.header.seq_num)
		clientTcb->sendBufTail = NULL;
	
	segBuf_t* bufPtr = clientTcb->sendBufHead;
	while (bufPtr && bufPtr->seg.header.seq_num < ack_seqnum) {
		clientTcb->sendBufHead = bufPtr->next;
		free(bufPtr);
		clientTcb->unAck_segNum--;
		bufPtr = clientTcb->sendBufHead;
	}
	pthread_mutex_unlock(clientTcb->bufMutex);
}


void sendBuf_clear(client_tcb_t* clientTcb) 
{
	pthread_mutex_lock(clientTcb->bufMutex);
    segBuf_t* bufPtr = clientTcb->sendBufHead;
	while(bufPtr!=clientTcb->sendBufTail) {
		segBuf_t* temp = bufPtr;
		bufPtr = bufPtr->next;
		free(temp);	
	}
	free(clientTcb->sendBufTail);
	clientTcb->sendBufunSent = NULL;
	clientTcb->sendBufHead = NULL;
  	clientTcb->sendBufTail = NULL;
	clientTcb->unAck_segNum = 0;
	pthread_mutex_unlock(clientTcb->bufMutex);
}