/**
 * @file    son/neighbortable.c
 * @brief   这个文件实现用于邻居表的API
 * @date    2023-02-23
 */


#include "neighbortable.h"
#include "../topology/topology.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>


//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create()
{
    // 解析文件topology/topology.dat
    topology_parseTopoDat();
    
    int nbrNum = topology_getNbrNum();
    if (nbrNum <= 0)
        return NULL;
    nbr_entry_t* nt = (nbr_entry_t*)malloc(sizeof(nbr_entry_t) * nbrNum);
    int* nbrID = topology_getNbrArray();
    in_addr_t* nbrIP = topology_getNbrIPArray();
    for (int i = 0; i < nbrNum; i++) {
        nt[i].nodeID = nbrID[i];
        nt[i].nodeIP = nbrIP[i];
        nt[i].conn = -1;
    }
    return nt;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
    int nbrNum = topology_getNbrNum();
    for (int i = 0; i < nbrNum; i++)
        close(nt[i].conn);
    if (nt)
        free(nt);
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
    int nbrNum = topology_getNbrNum();
    for (int i = 0; i < nbrNum; i++) {
        if (nt[i].nodeID == nodeID) {
            nt[i].conn = conn;
            return 1;
        }
    }
    return -1;
}