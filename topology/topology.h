/**
 * @file    topology/topology.h
 * @brief   这个文件声明一些用于解析拓扑文件的辅助函数
 * @date    2023-02-23
 */


#ifndef TOPOLOGY_H 
#define TOPOLOGY_H
#include <netdb.h>


#define TOPO_HOST_NUM 4


typedef struct linkedge {
    int to, cost, next;
} topo_edge_t;

/**
 * @brief   这个函数返回指定主机的节点ID.
 *          节点ID是节点IP地址最后8位表示的整数.
 *          例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
 *          如果不能获取节点ID, 返回-1.
 * 
 * @param hostname 
 * @return int 
 */
int topology_getNodeIDfromName(char* hostname); 


/**
 * @brief   这个函数返回指定的IP地址的节点ID.
 *          如果不能获取节点ID, 返回-1.
 * 
 * @param addr 
 * @return int 
 */
int topology_getNodeIDfromIP(struct in_addr* addr);


/**
 * @brief   这个函数返回本机的节点ID
 *          如果不能获取本机的节点ID, 返回-1.
 * 
 * @return int 
 */
int topology_getMyNodeID();


/**
 * @brief   这个函数解析保存在文件topology.dat中的拓扑信息.
 *          返回邻居数.
 * 
 * @return int 
 */
int topology_getNbrNum(); 


/**
 * @brief   这个函数解析保存在文件topology.dat中的拓扑信息.
 *          返回重叠网络中的总节点数.
 * 
 * @return int 
 */
int topology_getNodeNum();


/**
 * @brief   这个函数解析保存在文件topology.dat中的拓扑信息.
 *          返回一个动态分配的数组, 它包含重叠网络中所有节点的ID.  
 * 
 * @return int* 
 */
int* topology_getNodeArray(); 


/**
 * @brief   这个函数解析保存在文件topology.dat中的拓扑信息.
 *          返回一个动态分配的数组, 它包含所有邻居的节点ID.
 * 
 * @return int* 
 */ 
int* topology_getNbrArray(); 



/**
 * @brief   这个函数解析保存在文件topology.dat中的拓扑信息.
 *          返回一个动态分配的数组, 它包含所有邻居的节点IP.
 * 
 * @return in_addr_t* 
 */  
in_addr_t* topology_getNbrIPArray();

/**
 * @brief   这个函数解析保存在文件topology.dat中的拓扑信息.
 *          返回指定两个节点之间的直接链路代价. 
 *          如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
 * 
 * @param fromNodeID 
 * @param toNodeID 
 * @return unsigned int 
 */
unsigned int topology_getCost(int fromNodeID, int toNodeID);

int topology_parseTopoDat();
int topology_parseName(const char* hostname);
void add(int from, int to, int cost);
#endif