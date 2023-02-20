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
	unsigned short int checksum;  //这个段的校验和,本实验未使用
} stcp_hdr_t;


//段定义
typedef struct segment {
	stcp_hdr_t header;
	char data[MAX_SEG_LEN];
} seg_t;


//  客户端和服务器的SIP API 

/**
 * @brief 	SIP提供给STCP的send seg
 * @details	通过重叠网络(在本实验中，是一个TCP连接)发送STCP段. 因为TCP以字节流形式发送数据,
 * 			为了通过重叠网络TCP连接发送STCP段, 你需要在传输STCP段时，在它的开头和结尾加上分隔符.
 * 			即首先发送表明一个段开始的特殊字符"!&"; 然后发送seg_t; 最后发送表明一个段结束的特殊字符"!#".
 * 			成功时返回1, 失败时返回-1. sip_sendseg()首先使用send()发送两个字符, 然后使用send()发送seg_t,
 * 			最后使用send()发送表明段结束的两个字符.
 * 
 * @param connection 
 * @param segPtr 
 * @return int 
 */
int sip_sendseg(int connection, seg_t* segPtr);


/**
 * @brief 
 * 
 * @param connection 
 * @param segPtr 
 * @return int 
 */
int sip_recvseg(int connection, seg_t* segPtr);


/**
 * @brief 
 * @details	在剖析了一个STCP段之后,  调用seglost()来模拟网络中数据包的丢失.
 * 			如果段丢失了, 就返回1, 否则返回0.
 * 
 * @param segPtr 
 * @return int 
 */
int seglost(seg_t* segPtr); 


#endif