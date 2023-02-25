/**
 * @file    common/pkt.c
 * @brief   
 * @date    2023-02-23
 */


#include "pkt.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <string.h>

const char* BEGIN_SIGN = "!&";
const char* END_SIGN = "!#";
const char* PKT_TYPE[3] = {"", "ROUTE_UPDATE", "SIP"};

// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. 
// SON进程和SIP进程通过一个本地TCP连接互连.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
	if (send(son_conn, BEGIN_SIGN, 2, 0) <= 0) {
        printf("CONNECTION[SON_CONN] ERROR: [SIP] CAN'T [SEND] [BEGIN]\n");
		return -1;
	}
	if (send(son_conn, &nextNodeID, 4, 0) <= 0) {
        printf("CONNECTION[SON_CONN] ERROR: [SIP] CAN'T [SEND] [NEXT NODEID]\n");
		return -1;
	}
	if (send(son_conn, pkt, sizeof(sip_pkt_t), 0) <= 0) {
		printf("CONNECTION[SON_CONN] ERROR: [SIP] CAN'T [SEND] [PACKET]\n");
		return -1;
	}
	if (send(son_conn, END_SIGN, 2, 0) <= 0) {
        printf("CONNECTION[SON_CONN] ERROR: [SIP] CAN'T [SEND] [END]\n");
		return -1;
	}
	printf("PKT[%s] [SON_CONN] SEND: %d BYTES [SRC: %2d | DST: %2d]\n", 
		PKT_TYPE[pkt->header.type], pkt->header.length, pkt->header.src_nodeID, pkt->header.dest_nodeID);
	return 1;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
	int n,  state;
	char sign[3] = {0, 0, 0}, ch;

    // 确保以!&开始
	state = 0;
	while ((n = recv(son_conn, &ch, 1, 0)) == 1) {
		switch (state){
			case 0: state = (ch == '!' ? 1 : 0);
				break;
			case 1: state = (ch == '&' ? 2 : 0);
				break; 
		}
		if (state == 2) break;
	}
	if (n == 0)
		return 0;
	if (n < 0) {
		printf("CONNECTION[SON_CONN] ERROR: [SIP] CAN'T [RECV] [BEGIN]\n");
		return -1;
	}

    // 接收pkt
	if ((n = recv(son_conn, pkt, sizeof(sip_pkt_t), 0)) <= 0) {
		printf("CONNECTION[SON_CONN] ERROR: [SIP] CAN'T [RECV] [PACKET]\n");
		return -1;
	}

	// 确保以!#结尾
	if ((n = recv(son_conn, &sign, 2, 0)) == 2) {
		if (strcmp(sign, END_SIGN) != 0)
			return 0;
	} else if (n <= 0) {
		printf("CONNECTION[SON_CONN] ERROR: [SIP] CAN'T [RECV] [END]\n");
		return -1;
	}
	
    printf("PKT[%s] [SON_CONN] RECV: %d BYTES [SRC: %2d | DST: %2d]\n", 
		PKT_TYPE[pkt->header.type], pkt->header.length, pkt->header.src_nodeID, pkt->header.dest_nodeID);
    return 1;
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
	int n,  state;
	char sign[3] = {0, 0, 0}, ch;

    // 确保以!&开始
	state = 0;
	while ((n = recv(sip_conn, &ch, 1, 0)) == 1) {
		switch (state){
			case 0: state = (ch == '!' ? 1 : 0);
				break;
			case 1: state = (ch == '&' ? 2 : 0);
				break; 
		}
		if (state == 2) break;
	}
	if (n == 0)
		return 0;
	if (n < 0) {
		printf("CONNECTION[SIP_CONN] ERROR: [SON] CAN'T [RECV] [BEGIN]\n");
		return -1;
	}

    // 接收nextNode
	if ((n = recv(sip_conn, nextNode, 4, 0)) <= 0) {
		printf("CONNECTION[SIP_CONN] ERROR: [SON] CAN'T [RECV] [NEXT NODEID]\n");
		return -1;
	}

    // 接收pkt
	if ((n = recv(sip_conn, pkt, sizeof(sip_pkt_t), 0)) <= 0) {
		printf("CONNECTION[SIP_CONN] ERROR: [SON] CAN'T [RECV] [PACKET]\n");
		return -1;
	}
	
	// 确保以!#结尾
	if ((n = recv(sip_conn, &sign, 2, 0)) == 2) {
		if (strcmp(sign, END_SIGN) != 0)
			return 0;
	} else if (n <= 0) {
		printf("CONNECTION[SIP_CONN] ERROR: [SON] CAN'T [RECV] [END]\n");
		return -1;
	}
	
    printf("PKT[%s] [SIP_CONN] RECV: %d BYTES [SRC: %2d | DST: %2d]\n", 
		PKT_TYPE[pkt->header.type], pkt->header.length, pkt->header.src_nodeID, pkt->header.dest_nodeID);
    return 1;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
	if (send(sip_conn, BEGIN_SIGN, 2, 0) <= 0) {
        printf("CONNECTION[SIP_CONN] ERROR: [SON] CAN'T [SEND] [BEGIN]\n");
		return -1;
	}
	if (send(sip_conn, pkt, sizeof(sip_pkt_t), 0) <= 0) {
		printf("CONNECTION[SIP_CONN] ERROR: [SON] CAN'T [SEND] [PACKET]\n");
		return -1;
	}
	if (send(sip_conn, END_SIGN, 2, 0) <= 0) {
        printf("CONNECTION[SIP_CONN] ERROR: [SON] CAN'T [SEND] [END]\n");
		return -1;
	}
	printf("PKT[%s] [SIP_CONN] SEND: %d BYTES [SRC: %2d | DST: %2d]\n", 
		PKT_TYPE[pkt->header.type], pkt->header.length, pkt->header.src_nodeID, pkt->header.dest_nodeID);
	return 1;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
int sendpkt(sip_pkt_t* pkt, int conn)
{
	if (send(conn, BEGIN_SIGN, 2, 0) <= 0) {
        printf("CONNECTION[NXT_CONN] ERROR: [SON] CAN'T [SEND] [BEGIN]\n");
		return -1;
	}
	if (send(conn, pkt, sizeof(sip_pkt_t), 0) <= 0) {
		printf("CONNECTION[NXT_CONN] ERROR: [SON] CAN'T [SEND] [PACKET]\n");
		return -1;
	}
	if (send(conn, END_SIGN, 2, 0) <= 0) {
        printf("CONNECTION[NXT_CONN] ERROR: [SON] CAN'T [SEND] [END]\n");
		return -1;
	}
	printf("PKT[%s] [NXT_CONN] SEND: %d BYTES [SRC: %2d | DST: %2d]\n", 
		PKT_TYPE[pkt->header.type], pkt->header.length, pkt->header.src_nodeID, pkt->header.dest_nodeID);
	return 1;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符,报文通过SON进程和其邻居之间的TCP连接发送
int recvpkt(sip_pkt_t* pkt, int conn)
{
	int n,  state;
	char sign[3] = {0, 0, 0}, ch;

    // 确保以!&开始
	state = 0;
	while ((n = recv(conn, &ch, 1, 0)) == 1) {
		switch (state){
			case 0: state = (ch == '!' ? 1 : 0);
				break;
			case 1: state = (ch == '&' ? 2 : 0);
				break; 
		}
		if (state == 2) break;
	}
	if (n == 0)
		return 0;
	if (n < 0) {
		printf("CONNECTION[NXT_CONN] ERROR: [SON] CAN'T [RECV] [BEGIN]\n");
		return -1;
	}

    // 接收pkt
	if ((n = recv(conn, pkt, sizeof(sip_pkt_t), 0)) <= 0) {
		printf("CONNECTION[NXT_CONN] ERROR: [SON] CAN'T [RECV] [PACKET]\n");
		return -1;
	}
	
	// 确保以!#结尾
	if ((n = recv(conn, &sign, 2, 0)) == 2) {
		if (strcmp(sign, END_SIGN) != 0)
			return 0;
	} else if (n <= 0) {
		printf("CONNECTION[NXT_CONN] ERROR: [SON] CAN'T [RECV] [END]\n");
		return -1;
	}
	
    printf("PKT[%s] [NXT_CONN] RECV: %d BYTES [SRC: %2d | DST: %2d]\n", 
		PKT_TYPE[pkt->header.type], pkt->header.length, pkt->header.src_nodeID, pkt->header.dest_nodeID);
    return 1;
}