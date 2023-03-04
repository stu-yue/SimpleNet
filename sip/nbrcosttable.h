/**
 * @file    sip/nbrcosttable.h
 * @brief   这个文件定义用于邻居代价表的数据结构和函数.
 * @date    2023-02-25
 */


#ifndef NBRCOSTTABLE_H 
#define NBRCOSTTABLE_H 
#include <arpa/inet.h>

//邻居代价表条目定义
typedef struct neighborcostentry {
	unsigned int nodeID;	//邻居的节点ID
	unsigned int cost;	    //到该邻居的直接链路代价
} nbr_cost_entry_t;


/**
 * @brief   这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
 *          邻居的节点ID和直接链路代价提取自文件topology.dat.
 * 
 * @return nbr_cost_entry_t* 
 */
nbr_cost_entry_t* nbrcosttable_create();


/**
 * @brief   这个函数删除邻居代价表.
 *          它释放所有用于邻居代价表的动态分配内存.
 * 
 * @param nct 
 */
void nbrcosttable_destroy(nbr_cost_entry_t* nct);


/**
 * @brief   这个函数用于获取邻居的直接链路代价.
 *          如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
 * 
 * @param nct 
 * @param nodeID 
 * @return unsigned int 
 */
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID);


/**
 * @brief   这个函数打印邻居代价表的内容.
 * 
 * @param nct 
 */
void nbrcosttable_print(nbr_cost_entry_t* nct); 

#endif