/**
 * @file    son/son.h
 * @brief   这个文件定义用于SON进程的数据结构和函数
 * @date    2023-02-23
 */


#ifndef SON_H 
#define SON_H

#include "../common/constants.h"
#include "../common/pkt.h"
#include "neighbortable.h"


/**
 * @brief   这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
 *          在所有进入连接都建立后, 这个线程终止.
 * 
 * @param arg 
 * @return void* 
 */
void* waitNbrs(void* arg);


/**
 * @brief   这个函数连接到节点ID比自己小的所有邻居.
 *          在所有外出连接都建立后, 返回1, 否则返回-1.
 * 
 * @return int 
 */
int connectNbrs();


/**
 * @brief   这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
 *          在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 
 *          并将报文发送到重叠网络中的下一跳. 
 *          如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
 * 
 */
void waitSIP();


/**
 * @brief   每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
 *          所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的.
 * 
 * @param arg 
 * @return void* 
 */  
void* listen_to_neighbor(void* arg);


/**
 * @brief   这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
 *          它关闭所有的连接, 释放所有动态分配的内存.
 * 
 */
void son_stop(); 

#endif