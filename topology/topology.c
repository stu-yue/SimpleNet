/**
 * @file    topology/topology.c
 * @brief   这个文件实现一些用于解析拓扑文件的辅助函数
 * @date    2023-02-23
 */


#include "topology.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>    /* inet(3) functions */
#include <unistd.h>
#include <stdlib.h>
#include "../common/constants.h"


// 拓扑节点与IP对应关系
const char* TOPO_HOST_IP[TOPO_HOST_NUM][2] = { 
        {"netlab_1", "192.168.163.201"}, 
        {"netlab_2", "192.168.163.202"},
        {"netlab_3", "192.168.163.203"},
        {"netlab_4", "192.168.163.204"},
    };
int head[MAX_NODE_NUM], node_num, edge_cnt = 0;
topo_edge_t edges[MAX_NODE_NUM * MAX_NODE_NUM];


void add(int from, int to, int cost)
{
    edges[++edge_cnt].cost = cost;
    edges[edge_cnt].to = to;
    edges[edge_cnt].next = head[from];
    head[from] = edge_cnt;
}


int topology_parseName(const char* hostname)
{
    char hostnameCopy[256];
    int nodeID = 0;

    strcpy(hostnameCopy, hostname);
    char *p = strtok(hostnameCopy, "_");
    if (strcmp(p, "netlab") == 0) {
        p = strtok(NULL, "_");
        nodeID = atoi(p);
    }
    return nodeID;
}


int topology_parseTopoDat()
{
    FILE *fp;
    if ((fp = fopen("topology/topology.dat", "r")) == NULL) {
        printf("ERROR: CAN'T OPEN topology.dat\n");
        return -1;
    }

    while (!feof(fp)) {
        int node[2], linkcost;
        char host[2][256];
        fscanf(fp, "%s %s %d", host[0], host[1], &linkcost);
        node[0] = topology_parseName(host[0]);
        node[1] = topology_parseName(host[1]);
        if (node[0] > 0 && node[1] > 0) {
            add(node[0], node[1], linkcost);
            add(node[1], node[0], linkcost);
        }
    }
    fclose(fp);

    for (int i = 0; i < MAX_NODE_NUM; i++) {
        if (head[i] > 0) node_num++;
    }
    return node_num;
}


const char* getIPfromName(const char* hostname) 
{
    for (int i = 0; i < TOPO_HOST_NUM; i++) {
        if (strcmp(hostname, TOPO_HOST_IP[i][0]) == 0)
            return TOPO_HOST_IP[i][1];
    }
    return NULL;
}


int topology_getNodeIDfromName(char* hostname)
{
    int nodeID;
    if ((nodeID = topology_parseName(hostname)) > 0)
        return nodeID;
    return -1;
}


int topology_getNodeIDfromIP(struct in_addr* addr)
{
    int nodeID;
    char* str_addr = NULL;

    str_addr = inet_ntoa(*addr);
    if (str_addr) {
        for (int i = 0; i < TOPO_HOST_NUM; i++) {
            if (strcmp(str_addr, TOPO_HOST_IP[i][1]) == 0) {
                if ((nodeID = topology_parseName(TOPO_HOST_IP[i][0])) > 0)
                    return nodeID;
            }
        }
    } 
    return -1;
}


int topology_getMyNodeID()
{
    char hostname[256];
    if (gethostname(hostname, 256) != -1) {
        return topology_getNodeIDfromName(hostname);
    } else {
        printf("TOPO ERROR: CAN'T GET HOSTNAME\n");
        return -1;
    }
}


int topology_getNbrNum()
{
    int nodeID, nbrNum = 0;

    if ((nodeID = topology_getMyNodeID()) > 0) {
        for (int e = head[nodeID]; e != 0; e = edges[e].next) {
            nbrNum++;
        }
        return nbrNum;
    } else {
        printf("TOPO ERROR: NODEID IS INVALID\n");
        return -1;
    }
}


int topology_getNodeNum()
{
    return node_num;
}


int* topology_getNodeArray()
{
    int cnt = 0, *nodeArr;
    nodeArr = (int*)malloc(sizeof(int) * MAX_NODE_NUM);
    for (int i = 0; i < MAX_NODE_NUM; i++)
        if (head[i] > 0) nodeArr[cnt++] = i;
    return nodeArr;
}


int* topology_getNbrArray()
{
    int nodeID, cnt = 0, *nodeArr;
    nodeArr = (int*)malloc(sizeof(int) * MAX_NODE_NUM);
    if ((nodeID = topology_getMyNodeID()) > 0) {
        for (int e = head[nodeID]; e != 0; e = edges[e].next) {
            nodeArr[cnt++] = edges[e].to;
        }
        return nodeArr;
    } else {
        printf("TOPO ERROR: NODEID IS INVALID\n");
        return NULL;
    }
}

 
in_addr_t* topology_getNbrIPArray()
{
    int nodeID, cnt = 0;
    char nbrName[256] = "netlab_", nbrID[10];
    struct in_addr ip;
    in_addr_t *nodeArr;

    nodeArr = (in_addr_t*)malloc(sizeof(in_addr_t) * MAX_NODE_NUM);
    if ((nodeID = topology_getMyNodeID()) > 0) {
        for (int e = head[nodeID]; e != 0; e = edges[e].next) {
            nbrName[7] = 0;
            sprintf(nbrID, "%d", edges[e].to);
            strcat(nbrName, nbrID);
            inet_aton(getIPfromName(nbrName), &ip);
            nodeArr[cnt++] = ip.s_addr;
        }
        return nodeArr;
    } else {
        printf("TOPO ERROR: NODEID IS INVALID\n");
        return NULL;
    }
}


unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
    for (int e = head[fromNodeID]; e != 0; e = edges[e].next) {
        if (edges[e].to == toNodeID)
            return edges[e].cost;
    }
    return INFINITE_COST;
}