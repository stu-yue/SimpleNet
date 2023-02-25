/**
 * @file 	seg.h
 * @brief 	这个文件包含STCP段定义, 以及用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的原型.
 * @date 	2023-2-18
 */

#ifndef SEG_H
#define SEG_H

#include "constants.h"


//段类型定义, 用于STCP.
#define	SYN 0
#define	SYNACK 1
#define	FIN 2
#define	FINACK 3
#define	DATA 4
#define	DATAACK 5


//段首部定义 
typedef struct stcp_hdr {
	unsigned int src_port;        //源端口号
	unsigned int dest_port;       //目的端口号
	unsigned int seq_num;         //序号
	unsigned int ack_num;         //确认号
	unsigned short int length;    //段数据长度
	unsigned short int  type;     //段类型
	unsigned short int  rcv_win;  //本实验未使用
	unsigned short int checksum;  //这个段的校验和
} stcp_hdr_t;

//段定义
typedef struct segment {
	stcp_hdr_t header;
	char data[MAX_SEG_LEN];
} seg_t;

//这是在SIP进程和STCP进程间交换的数据结构
typedef struct sendsegargument {
	int nodeID;		//节点ID 
	seg_t seg;		//一个段 
} sendseg_arg_t;


/* 客户端和服务器的SIP API */

/**
 * @brief 	STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
 * @details	通过重叠网络发送STCP段. 因为TCP以字节流形式发送数据,
 * 			为了通过重叠网络TCP连接发送STCP段, 你需要在传输STCP段时，在它的开头和结尾加上分隔符.
 * 			即首先发送表明一个段开始的特殊字符"!&"; 然后发送seg_t; 最后发送表明一个段结束的特殊字符"!#".
 * 			成功时返回1, 失败时返回-1. sip_sendseg()首先使用send()发送两个字符, 然后使用send()发送seg_t,
 * 			最后使用send()发送表明段结束的两个字符.
 * 
 * @param sip_conn
 * @param dest_nodeID
 * @param segPtr 
 * @return int 
 */
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr);


/**
 * @brief   STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
 * 
 * @param sip_conn 
 * @param src_nodeID
 * @param segPtr 
 * @return int 
 */
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr);


/**
 * @brief   SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
 * 
 * @param stcp_conn 
 * @param dest_nodeID 
 * @param segPtr 
 * @return int 
 */
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr); 


/**
 * @brief   SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
 * 
 * @param stcp_conn 
 * @param src_nodeID 
 * @param segPtr 
 * @return int 
 */
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr); 

/**
 * @brief 
 * @details	在剖析了一个STCP段之后,  调用seglost()来模拟网络中数据包的丢失.
 * 			如果段丢失了, 就返回1, 否则返回0.
 * 
 * @param segPtr 
 * @return int 
 */
int seglost(seg_t* segPtr); 


/**
 * @brief 	这个函数计算指定段的校验和.
 * @details	校验和计算覆盖段首部和段数据. 首先将段首部中的校验和字段清零, 
 * 			如果数据长度为奇数, 添加一个全零的字节来计算校验和.
 * 			校验和计算使用1的补码.
 * 
 * @param segment 
 * @return unsigned short 
 */
unsigned short checksum(seg_t* segment);


/**
 * @brief 	这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
 * 
 * @param segment 
 * @return int 
 */
int checkchecksum(seg_t* segment);

#endif