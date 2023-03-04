/**
 * @file 	stcp_client.h
 * @brief 	这个文件包含客户端状态定义, 一些重要的数据结构和客户端STCP套接字接口定义.
 * @date 	2023-2-18
 */

#ifndef STCPCLIENT_H
#define STCPCLIENT_H

#include <pthread.h>
#include "../common/seg.h"

//FSM中使用的客户端状态
#define	CLOSED 1
#define	SYNSENT 2
#define	CONNECTED 3
#define	FINWAIT 4

//在发送缓冲区链表中存储段的单元
typedef struct segBuf {
        seg_t seg;
        unsigned int sentTime;
        struct segBuf* next;
} segBuf_t;


// 客户端传输控制块. 一个STCP连接的客户端使用这个数据结构记录连接信息.
typedef struct client_tcb {
	unsigned int server_nodeID;        	//服务器节点ID, 类似IP地址, 本实验未使用
	unsigned int server_portNum;       	//服务器端口号
	unsigned int client_nodeID;     	//客户端节点ID, 类似IP地址, 本实验未使用
	unsigned int client_portNum;    	//客户端端口号
	unsigned int state;     			//客户端状态
	unsigned int next_seqNum;       	//新段准备使用的下一个序号 
	pthread_mutex_t* bufMutex;      	//发送缓冲区互斥量
	segBuf_t* sendBufHead;          	//发送缓冲区头
	segBuf_t* sendBufunSent;        	//发送缓冲区中的第一个未发送段
	segBuf_t* sendBufTail;          	//发送缓冲区尾
	unsigned int unAck_segNum;      	//已发送但未收到确认段的数量
} client_tcb_t;


//  用于客户端应用程序的STCP套接字API. 
//  ===================================
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现. 
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/**
 * @brief 	客户端STCP层的初始化函数
 * @details	这个函数初始化TCB表, 将所有条目标记为NULL.
 * 			它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量，该变量作为sip_sendseg和sip_recvseg的输入参数.
 * 			最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
 * 
 * @param conn 重叠网络的TCP套接字描述符
 */
void stcp_client_init(int conn);


/**
 * @brief 	客户端STCP层的sock函数
 * @details 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
 * 			该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
 * 			TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
 * 			如果TCB表中没有条目可用, 这个函数返回-1.
 * 
 * @param client_port 待绑定的客户端端口
 * @return int 套接字描述符/-1 
 */
int stcp_client_sock(unsigned int client_port);


/**
 * @brief 	客户端STCP层的connect函数
 * @details	这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
 * 			这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
 * 			在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN段将被重传. 
 * 			如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
 * 
 * @param socked 
 * @param nodeID
 * @param server_port 
 * @return int 
 */
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port);


/**
 * @brief 
 * 
 * @param sockfd 
 * @param data 
 * @param length 
 * @return int 
 */
int stcp_client_send(int sockfd, void* data, unsigned int length);

/**
 * @brief 	客户端STCP层的disconnect函数
 * @details	这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.
 * 			这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
 * 			如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
 * 			state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
 * 
 * @param sockfd 
 * @return int 
 */

int stcp_client_disconnect(int sockfd);


/**
 * @brief 	客户端STCP层的close函数
 * @details 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
 * 			失败时(即位于错误的状态)返回-1.
 * 
 * @param sockfd 
 * @return int 
 */
int stcp_client_close(int sockfd);


/**
 * @brief 
 * @details 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段.
 * 			seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
 * 			线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
 * 
 * @param arg 
 * @return void* 
 */
void *seghandler(void* arg);


/**
 * @brief 	
 * @details 如果发送缓冲区非空, 它应一直运行.
 * 			如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
 * 			当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
 * 
 * @param clienttcb 
 * @return void* 
 */
void* sendBuf_timer(void* clienttcb);


// 

/**
 * @brief 向缓冲区添加数据段
 * 
 * @param clientTcb 
 * @param newSegBuf 
 */
void sendBuf_addSeg(client_tcb_t* clientTcb, segBuf_t* newSegBuf);


/**
 * @brief 超时重发
 * 
 * @param clientTcb 
 */
void sendBuf_timeout(client_tcb_t* clientTcb);


/**
 * @brief 从第一个未被发送的段开始发送，直到已发送但未被确认的段数量到达 GBN_WINDOW 或是缓冲区未发送段全部发送为止
 * 
 * @param clientTcb 
 */
void sendBuf_send(client_tcb_t* clientTcb);


/**
 * @brief 接收到一个有效的 DATAACK 段，将被确认的段从发送缓冲区中删除并释放
 * 
 * @param clientTcb 
 * @param ack_seqnum 
 */
void sendBuf_recvAck(client_tcb_t* clientTcb, unsigned int ack_seqnum);


/**
 * @brief 当客户端TCB在stcp_client_disconnect()中转换到CLOSED状态时，发送缓冲区应被清空
 * 
 * @param clientTcb 
 */
void sendBuf_clear(client_tcb_t* clientTcb);

#endif
