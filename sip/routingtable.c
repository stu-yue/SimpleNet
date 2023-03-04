/**
 * @file    sip/routingtable.h
 * @brief   这个文件实现用于路由表的数据结构和函数.
 * @date    2023-02-25
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"


int makehash(int node)
{
    return node % MAX_ROUTINGTABLE_SLOTS;
}


routingtable_t* routingtable_create()
{
    routingtable_t* routingtable = (routingtable_t*)malloc(sizeof(routingtable_t));
    for (int i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++)
        routingtable->hash[i] = NULL;
  
    int* nbrArr = topology_getNbrArray();
    for (int i = 0; i < topology_getNbrNum(); i++) {
        int slotIdx = makehash(nbrArr[i]);
        if (routingtable->hash[slotIdx] == NULL) {
            routingtable->hash[slotIdx] = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
            routingtable->hash[slotIdx]->destNodeID = nbrArr[i];
            routingtable->hash[slotIdx]->nextNodeID = nbrArr[i];
            routingtable->hash[slotIdx]->next = NULL;
        } else {
            routingtable_entry_t* entry = routingtable->hash[slotIdx];
            routingtable->hash[slotIdx] = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
            routingtable->hash[slotIdx]->destNodeID = nbrArr[i];
            routingtable->hash[slotIdx]->nextNodeID = nbrArr[i];
            routingtable->hash[slotIdx]->next = entry;
        }
    }
    return routingtable;
}


void routingtable_destroy(routingtable_t* routingtable)
{
    for (int  i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
        if (routingtable->hash[i]) {
            routingtable_entry_t* entry = routingtable->hash[i];
            while (entry) {
                routingtable_entry_t* tmp = entry;
                entry = entry->next;
                free(tmp);
            }
        }
    }
    free(routingtable);
}


void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
    int slotIdx = makehash(destNodeID);
    if (routingtable->hash[slotIdx] == NULL) {
        routingtable->hash[slotIdx] = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
        routingtable->hash[slotIdx]->destNodeID = destNodeID;
        routingtable->hash[slotIdx]->nextNodeID = nextNodeID;
        routingtable->hash[slotIdx]->next = NULL;
    } else {
        routingtable_entry_t* entry = routingtable->hash[slotIdx];
        while (entry) {
            if (entry->destNodeID == destNodeID) {
                entry->nextNodeID = nextNodeID; 
                return;
            }
            entry = entry->next;
        }
        entry = routingtable->hash[slotIdx];
        routingtable->hash[slotIdx] = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
        routingtable->hash[slotIdx]->destNodeID = destNodeID;
        routingtable->hash[slotIdx]->nextNodeID = nextNodeID;
        routingtable->hash[slotIdx]->next = entry;
    }
}


int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
    int slotIdx = makehash(destNodeID);
    if (routingtable->hash[slotIdx]) {
        routingtable_entry_t* entry = routingtable->hash[slotIdx];
        while (entry) {
            if (entry->destNodeID == destNodeID)
                return entry->nextNodeID;
            entry = entry->next;
        }
    }
    return -1;
}


void routingtable_print(routingtable_t* routingtable)
{
    printf("ROUTING TABLE PRINT:\n");
    for (int i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++) {
        printf("SRC[%d]: ", i);
        if (routingtable->hash[i]) {
            routingtable_entry_t* entry = routingtable->hash[i];
            while (entry->next) {
                printf("[DEST: %d | NEXT: %d] --> ", entry->destNodeID, entry->nextNodeID);
                entry = entry->next;
            }
            printf("[DEST: %d | NEXT: %d]\n", entry->destNodeID, entry->nextNodeID);
        } else {
            printf("NULL\n");
        }
    }
}