/**
 * @file    sip/nbrcosttable.c
 * @brief   这个文件实现用于邻居代价表的数据结构和函数.
 * @date    2023-02-25
 */


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

 
nbr_cost_entry_t* nbrcosttable_create()
{
    int myNodeID = topology_getMyNodeID();
    int* nbrArr = topology_getNbrArray();
    int nbrNum = topology_getNbrNum();
    nbr_cost_entry_t* nct = (nbr_cost_entry_t*)malloc(sizeof(nbr_cost_entry_t) * nbrNum);
    for (int i = 0; i < nbrNum; i++) {
        nct[i].nodeID = nbrArr[i];
        nct[i].cost = topology_getCost(myNodeID, nbrArr[i]);
    }
    return nct;
}


void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
    if (nct)
        free(nct);
}


unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
    int nbrNum = topology_getNbrNum();
    for (int i = 0; i < nbrNum; i++) {
        if (nodeID == nct[i].nodeID)
            return nct[i].cost;
    }
    return INFINITE_COST;
}


void nbrcosttable_print(nbr_cost_entry_t* nct)
{
    int nbrNum = topology_getNbrNum();
    printf("---------NEIGHBOR COST TABLE---------\n");
    for (int i = 0; i < nbrNum; i++) {
        printf("NBR_COST[%d]: [NODEID: %d | COST: %d]\n", i, nct[i].nodeID, nct[i].cost);
    }
    printf("-------------------------------------\n");
}