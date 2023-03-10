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
// 全局变量访问锁
pthread_mutex_t son_mutex;

/* 实现重叠网络函数 */

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接
void* waitNbrs(void* arg) 
{
	int myNodeID = topology_getMyNodeID();
	int nbrNum = topology_getNbrNum();
	listenfd = tcp_server_listen(CONNECTION_PORT);
	if (listenfd == -1) {
		printf("SON: BIND LISTENFD FAILED\n");
		pthread_exit(NULL);
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
			} else {
				// 成功连接请求，则建立监听线程
				for (int i = 0; i < nbrNum; i++) {
					if (nt[i].nodeIP == client_addr.sin_addr.s_addr) {
						nt[i].conn = connfd;
						int* idx = (int*)malloc(sizeof(int));
						*idx = i;
						pthread_t nbr_listen_thread;
						pthread_create(&nbr_listen_thread, NULL, listen_to_neighbor, (void*)idx);
						printf("SON: NODE[%d] IS ACCEPTED NEIGHBOR[%d]\n", myNodeID, nt[i].nodeID);
					}
				}
				// 打印邻居表
				for(int i = 0; i < nbrNum; i++) {
					printf("OVERLAY NETWORK: NEIGHBOR[%d] | NODEID[%d] | NODEIP[%8d] | CONN[%d]\n", 
						i + 1, nt[i].nodeID, nt[i].nodeIP, nt[i].conn);
				}		
			}
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
			if ((nt[i].conn = tcp_client_conn_n(nt[i].nodeIP, CONNECTION_PORT)) < 0) {
				return -1;
			} else {
				// 请求成功，建立监听线程
				int* idx = (int*)malloc(sizeof(int));
				*idx = i;
				pthread_t nbr_listen_thread;
				pthread_create(&nbr_listen_thread, NULL, listen_to_neighbor, (void*)idx);
				printf("SON: NODE[%d] CONNECT TO NEIGHBOR[%d]\n", myNodeID, nt[i].nodeID);
			}
		}
	}
	printf("SON: CONNECT NBRS IS OVER..\n");
	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) 
{
	int *idx = (int*)arg, n;
	printf("SON: LISTENG THREAD TO NEIGHBOR[%d]\n", *idx);
	sip_pkt_t pkt;
	while (1) {
		if ((n = recvpkt(&pkt, nt[*idx].conn)) > 0) {
			if (forwardpktToSIP(&pkt, sip_conn) < 0)
				sip_conn = -1;
		} else if (n < 0) {
			printf("n:%d\n", n);
			printf("SON: NEIGHBOR[%d] IS DISCONNECTED\n", *idx + 1);
			nt[*idx].conn = -1;
			pthread_exit(NULL);
		}
	}
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() 
{
	printf("SON: WAIT SIP...\n");
	int sip_listenfd = tcp_server_listen(SON_PORT);
	if (sip_listenfd == -1) {
		printf("SON: BIND SIP_LISTENFD FAILED\n");
		pthread_exit(NULL);
	}

	sip_pkt_t pkt;
	int nextNode, n;
	fd_set readmask, allreads;
	FD_ZERO(&allreads);
	FD_SET(sip_listenfd, &allreads);
	while (1) {
		if (sip_conn <= 0) {
			readmask = allreads;
			select(sip_listenfd + 1, &readmask, NULL, NULL, &(struct timeval){.tv_usec = 1e5});
			if (FD_ISSET(sip_listenfd, &readmask)) {
				struct sockaddr_in client_addr;
				socklen_t client_len = sizeof(client_addr);
				if ((sip_conn = accept(sip_listenfd, (struct sockaddr *) &client_addr, &client_len)) < 0) {
					printf("SON: SERVER ACCEPT FAILED\n");
				} else {
					printf("SON: SIP PROCESS IS ACCEPTED\n");
				}
			}
		}
		if (sip_conn <= 0) continue;
		if ((n = getpktToSend(&pkt, &nextNode, sip_conn)) > 0) {
			if (pkt.header.dest_nodeID == BROADCAST_NODEID) {
				printf("SON: BROADCAST\n");
				int nbrNum = topology_getNbrNum();
				for (int i = 0; i < nbrNum; i++) {
					if (nt[i].conn > 0)
						if (sendpkt(&pkt, nt[i].conn) < 0)
							nt[i].conn = -1;
				}
			} else {
				int nbrNum = topology_getNbrNum();
				for (int i = 0; i < nbrNum; i++) {
					if (nt[i].nodeID == nextNode && nt[i].conn > 0)
						if (sendpkt(&pkt, nt[i].conn) < 0)
							nt[i].conn = -1;
				}
			}
		} else if (n <= 0) {
			printf("SON: SIP PROCESS IS DISCONNECTED\n");
			sip_conn = -1;
		}
	}
}


void son_stop() 
{
	printf("SON: CLOSE SIP_CONN\n");
	nt_destroy(nt);
	close(sip_conn);
	close(listenfd);
	exit(0);
}


int main() 
{
	

	//启动重叠网络初始化工作
	printf("OVERLAY NETWORK: NODE[%d] INITIALIZING...\n", topology_getMyNodeID());	

	// 解析文件topology/topology.dat
    topology_parseTopoDat();

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	//初始化全局变量访问锁
	pthread_mutex_init(&son_mutex, NULL);
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);
	signal(SIGKILL, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	for(int i = 0; i < nbrNum; i++) {
		printf("OVERLAY NETWORK: NEIGHBOR[%d] | NODEID[%d] | NODEIP[%8d] | CONN[%d]\n", 
			i + 1, nt[i].nodeID, nt[i].nodeIP, nt[i].conn);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread, NULL, waitNbrs, (void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	// pthread_join(waitNbrs_thread, NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	
	//创建线程监听所有邻居
	// for(i = 0; i < nbrNum; i++) {
	// 	int* idx = (int*)malloc(sizeof(int));
	// 	*idx = i;
	// 	pthread_t nbr_listen_thread;
	// 	pthread_create(&nbr_listen_thread, NULL, listen_to_neighbor, (void*)idx);
	// }

	//等待与邻居间连接建立好
	sleep(SON_START_DELAY / 2);
	// 打印邻居表
	for(int i = 0; i < nbrNum; i++) {
		printf("OVERLAY NETWORK: NEIGHBOR[%d] | NODEID[%d] | NODEIP[%8d] | CONN[%d]\n", 
			i + 1, nt[i].nodeID, nt[i].nodeIP, nt[i].conn);
	}	

	printf("OVERLAY NETWORK: NODE[%d] INITIALIZED...\n", topology_getMyNodeID());
	printf("OVERLAY NETWORK: WAITING FOR CONNECTION FROM SIP PROCESSS...\n");

	//等待来自SIP进程的连接
	waitSIP();
}