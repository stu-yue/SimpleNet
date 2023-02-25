/**
 * @file    son/son.c
 * @brief   这个文件实现SON进程
 * @date    2023-02-23
 */


#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/tcp.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

// 在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 60

/* 声明全局变量 */

// 将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
// 将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 
int listenfd;

/* 实现重叠网络函数 */

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
void* waitNbrs(void* arg) 
{
	int myNodeID = topology_getMyNodeID();
	int nbrNum = topology_getNbrNum();
	listenfd = tcp_server_listen(CONNECTION_PORT);
	if (listenfd == -1) {
		printf("SON: WRONG\n");
	}
	fd_set readmask, allreads;
	FD_ZERO(&allreads);
	FD_SET(listenfd, &allreads);
	while (1) {
		readmask = allreads;
		select(listenfd + 1, &readmask, NULL, NULL, &(struct timeval){.tv_usec = 1e5});
		if (FD_ISSET(listenfd, &readmask)) {
			int connfd;
			struct sockaddr_in client_addr;
			socklen_t client_len = sizeof(client_addr);
			if ((connfd = accept(listenfd, (struct sockaddr *) &client_addr, &client_len)) < 0) {
				printf("SON: SERVER ACCEPT FAILED\n");
				pthread_exit(NULL);
			}
			for (int i = 0; i < nbrNum; i++) {
				if (nt[i].nodeIP == client_addr.sin_addr.s_addr) {
					nt[i].conn = connfd;
					printf("SON: NODE[%d] IS ACCEPTED NEIGHBOR[%d]\n", myNodeID, nt[i].nodeID);
				}
			}
		}
		int flag = 0;
		for (int i = 0; i < nbrNum; i++) {
			if (nt[i].nodeID > myNodeID && nt[i].conn == -1)
				flag = 1;
		}
		if (!flag) {
			printf("SON: WAIT NBRS IS OVER...\n");
			pthread_exit(NULL);
		}
	}
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() 
{
	int myNodeID = topology_getMyNodeID();
	int nbrNum = topology_getNbrNum();
	for (int i = 0; i < nbrNum; i++) {
		if (nt[i].nodeID < myNodeID) {
			printf("SON: NODE[%d] PREPARE TO CONNECT NODE[%d]...\n", myNodeID, nt[i].nodeID);
			if ((nt[i].conn = tcp_client_conn_n(nt[i].nodeIP, CONNECTION_PORT)) < 0)
				return -1;
			printf("SON: NODE[%d] CONNECT TO NEIGHBOR[%d]\n", myNodeID, nt[i].nodeID);
		}
	}
	printf("SON: CONNECT NBRS IS OVER..\n");
	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) 
{
	int* idx = (int*)arg;
	printf("SON: LISTENG IDX [%d]\n", *idx);
	sip_pkt_t pkt;
	while (1) {
		if (recvpkt(&pkt, nt[*idx].conn) > 0) {
			forwardpktToSIP(&pkt, sip_conn);
		}
	}
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() 
{
	printf("SON: WAIT SIP...\n");
	sip_conn = tcp_server_conn(SON_PORT);
	sip_pkt_t pkt;
	int nextNode;
	while (1) {
		if (getpktToSend(&pkt, &nextNode, sip_conn) > 0) {
			if (pkt.header.dest_nodeID == BROADCAST_NODEID) {
				int nbrNum = topology_getNbrNum();
				for (int i = 0; i < nbrNum; i++) {
					sendpkt(&pkt, nt[i].conn);
					printf("son: recv BROADCAST_BODEID from sip\n");
				}
			}
			else {
				int nbrNum = topology_getNbrNum();
				for (int i = 0; i < nbrNum; i++) {
					if (nt[i].nodeID == nextNode)
						sendpkt(&pkt, nt[i].conn);
				}
			}
		}
	}
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() 
{
	nt_destroy(nt);
	close(sip_conn);
	close(listenfd);
	exit(0);
}


int main() 
{
	

	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n", topology_getMyNodeID());	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	int i;
	for(i = 0; i < nbrNum; i++) {
		printf("Overlay network: neighbor %d:%d\n", i+1, nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread, NULL, waitNbrs, (void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread, NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	
	//创建线程监听所有邻居
	for(i = 0; i < nbrNum; i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread, NULL, listen_to_neighbor, (void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}