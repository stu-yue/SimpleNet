/**
 * @file    sip/sip.c
 * @brief   这个文件实现SIP进程
 * @date    2023-02-23
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>
#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../common/tcp.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"


//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 60

/* 声明全局变量 */
int son_conn; 							//到重叠网络的连接
int stcp_conn;							//到STCP的连接
nbr_cost_entry_t* nct;					//邻居代价表
dv_t* dv;								//距离矢量表
pthread_mutex_t* dv_mutex;				//距离矢量表互斥量
routingtable_t* routingtable;			//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/* 实现SIP的函数 */

int connectToSON() 
{
	return tcp_client_conn_a("127.0.0.1", SON_PORT);
}


void* routeupdate_daemon(void* arg) 
{
	while (1) {
		select(0, 0, 0, 0, &(struct timeval){.tv_sec = ROUTEUPDATE_INTERVAL});
		
		pthread_mutex_lock(dv_mutex);
		int myNodeID = topology_getMyNodeID();
		int* nodeArr = topology_getNodeArray();
		pkt_routeupdate_t pkt_rp;
		pkt_rp.entryNum = 0;
		for (int i = 0; i < topology_getNodeNum(); i++) {
			pkt_rp.entry[pkt_rp.entryNum].nodeID = nodeArr[i];
			pkt_rp.entry[pkt_rp.entryNum].cost = dvtable_getcost(dv, myNodeID, nodeArr[i]);
			pkt_rp.entryNum++;
		}
		pthread_mutex_unlock(dv_mutex);

		sip_pkt_t pkt;
		pkt.header.src_nodeID = myNodeID;
		pkt.header.dest_nodeID = BROADCAST_NODEID;
		pkt.header.type = ROUTE_UPDATE;
		pkt.header.length = sizeof(pkt_rp);
		memcpy(pkt.data, &pkt_rp, pkt.header.length);
		son_sendpkt(BROADCAST_NODEID, &pkt, son_conn);
	}
}


void* pkthandler(void* arg) 
{
	sip_pkt_t pkt;
	seg_t seg;
	pkt_routeupdate_t pkt_rp;
	while (1) {
		if (son_recvpkt(&pkt, son_conn) > 0) {
			if (pkt.header.type == SIP) {
				if (pkt.header.dest_nodeID == topology_getMyNodeID()) {
					memcpy(&seg, pkt.data, pkt.header.length);
					forwardsegToSTCP(stcp_conn, pkt.header.src_nodeID, &seg);
				} else {
					pthread_mutex_lock(routingtable_mutex);
					int next_NodeID = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
					pthread_mutex_unlock(routingtable_mutex);
					if (next_NodeID != -1) {
						printf("FROWARD: from Node_%d to Node_%d\n", pkt.header.src_nodeID, pkt.header.dest_nodeID);
						son_sendpkt(next_NodeID, &pkt, son_conn);
					}
				}
			} else if (pkt.header.type == ROUTE_UPDATE) {
				int src_nodeID = pkt.header.src_nodeID;
				memcpy(&pkt_rp, pkt.data, pkt.header.length);
				pthread_mutex_lock(dv_mutex);
				for (int i = 0; i < pkt_rp.entryNum; i++) {
					dvtable_setcost(dv, src_nodeID, pkt_rp.entry[i].nodeID, pkt_rp.entry[i].cost);
				}
				int* nbrArr = topology_getNbrArray();
				int* nodeArr = topology_getNodeArray();
				int x = topology_getMyNodeID();
				// 更新距离向量
				for (int i = 0; i < topology_getNodeNum(); i++) {
					int y = nodeArr[i];
					for (int j = 0; j < topology_getNbrNum(); j++) {
						int v = nbrArr[j];
						int cost_xv = dvtable_getcost(dv, x, v);
						int cost_vy = dvtable_getcost(dv, v, y);
						if (cost_xv + cost_vy < dvtable_getcost(dv, x, y)) {
							dvtable_setcost(dv, x, y, cost_xv + cost_vy);
							// 更新路由表
							pthread_mutex_lock(routingtable_mutex);
							routingtable_setnextnode(routingtable, y, v);
							pthread_mutex_unlock(routingtable_mutex);
						}
					}
				}
				pthread_mutex_unlock(dv_mutex);
			}
		}
	}
}


//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop()
{
	printf("SIP: CLOSE SON_CONN\n");
	close(son_conn);
	close(stcp_conn);
	nbrcosttable_destroy(nct);
	dvtable_destroy(dv);
	routingtable_destroy(routingtable);
	free(dv_mutex);
	free(routingtable_mutex);
	exit(0);
}


void waitSTCP() 
{
	printf("WAIT SIP...\n");
	int listenfd = tcp_server_listen(SIP_PORT);
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	if ((stcp_conn = accept(listenfd, (struct sockaddr*) &client_addr, &client_len)) < 0) {
		printf("SIP: ACCEPT FAILED\n");
		exit(0);
	}
	sip_pkt_t pkt;
	seg_t seg;
	int dest_nodeID;
	while (1) {
		if (getsegToSend(stcp_conn, &dest_nodeID, &seg) > 0) {
			pthread_mutex_lock(routingtable_mutex);
			int next_nodeID = routingtable_getnextnode(routingtable, dest_nodeID);
			pthread_mutex_unlock(routingtable_mutex);
			if (next_nodeID != -1) {
				pkt.header.src_nodeID = topology_getMyNodeID();
				pkt.header.dest_nodeID = dest_nodeID;
				pkt.header.length = sizeof(stcp_hdr_t) + seg.header.length;
				pkt.header.type = SIP;
				memcpy(pkt.data, &seg, pkt.header.length);
				son_sendpkt(next_nodeID, &pkt, son_conn);
			}
		} else {
			printf("SIP: STCP DISCONNECTED\n");
			close(stcp_conn);
			if ((stcp_conn = accept(listenfd, (struct sockaddr*) &client_addr, &client_len)) < 0) {
				printf("SIP: ACCEPT FAILED\n");
			}
		}
	}
}


int main(int argc, char *argv[]) 
{

	printf("SIP LAYER IS STARTING, PLEASE WAIT...\n");


	//初始化全局变量
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);
	signal(SIGKILL, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();

	if (son_conn < 0) {
		printf("CAN'T CONNECT TO SON PROCESS\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread, NULL, pkthandler, (void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread, NULL, routeupdate_daemon, (void*)0);	


	printf("SIP LAYER IS STARTED...\n");


	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 
}