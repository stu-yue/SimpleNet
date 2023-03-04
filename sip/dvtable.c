/**
 * @file    sip/dvtable.C
 * @brief   这个文件实现用于距离矢量表的数据结构和函数.
 * @date    2023-02-25
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"


dv_t* dvtable_create()
{
    int myNodeID = topology_getMyNodeID();
    int* nbrArr = topology_getNbrArray();
    int* nodeArr = topology_getNodeArray();
    int nbrNum = topology_getNbrNum();
    int nodeNum = topology_getNodeNum();
    dv_t* dv = (dv_t*)malloc(sizeof(dv_t) * (nbrNum + 1));
    for (int i = 0; i <= nbrNum; i++) {
        dv[i].nodeID = i< nbrNum ? nbrArr[i] : myNodeID;
        dv[i].dvEntry = (dv_entry_t*)malloc(sizeof(dv_entry_t) * (nodeNum));
        for (int j = 0; j < nodeNum; j++) {
            int src_nodeID = dv[i].nodeID;
            int dest_nodeID = nodeArr[j];
            dv[i].dvEntry[j].nodeID = dest_nodeID;
            dv[i].dvEntry[j].cost = src_nodeID == dest_nodeID ? 0 : topology_getCost(src_nodeID, dest_nodeID);
        }
    }
    return dv;
}


void dvtable_destroy(dv_t* dvtable)
{
    for (int i = 0; i <= topology_getNbrNum(); i++)
        free(dvtable[i].dvEntry);
    free(dvtable);
}


int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
    for (int i = 0; i <= topology_getNbrNum(); i++) {
        if (dvtable[i].nodeID == fromNodeID) {
            for (int j = 0; j <= topology_getNodeNum(); j++) {
                if (dvtable[i].dvEntry[j].nodeID == toNodeID) {
                    dvtable[i].dvEntry[j].cost = cost;
                    return 1;
                }
            }
        }
    }
    return -1;
}


unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
    for (int i = 0; i <= topology_getNbrNum(); i++) {
        if (dvtable[i].nodeID == fromNodeID) {
            for (int j = 0; j <= topology_getNodeNum(); j++)
                if (dvtable[i].dvEntry[j].nodeID == toNodeID)
                    return dvtable[i].dvEntry[j].cost;
        }
    }
    return INFINITE_COST;
}


void dvtable_print(dv_t* dvtable)
{
    printf("DISTANCE VECTOR TABLE PRINT:\n");
    for (int i = 0; i <= topology_getNbrNum(); i++) {
        printf("SRC[%d]: ", dvtable[i].nodeID);
        for (int j = 0; j < topology_getNodeNum(); j++)
            printf("[DEST: %d | COST: %d] ", dvtable[i].dvEntry[j].nodeID, dvtable[i].dvEntry[j].cost);
        printf("\n");
    }
}