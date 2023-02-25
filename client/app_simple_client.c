/**
 * @file    client/app_simple_client.c
 * @brief 	这是简单版本的客户端程序代码. 客户端首先通过在客户端和服务器之间创建TCP连接,启动重叠网络层. 
 *          然后它调用stcp_client_init()初始化STCP客户端. 它通过两次调用stcp_client_sock()和stcp_client_connect()创建两个套接字并连接到服务器.
 *          它然后通过这两个连接发送一段短的字符串给服务器. 经过一段时候后, 客户端调用stcp_client_disconnect()断开到服务器的连接.
 *          最后,客户端调用stcp_client_close()关闭套接字. 重叠网络层通过调用son_stop()停止.
 * @date    2023-02-21
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../common/constants.h"
#include "../common/tcp.h"
#include "../topology/topology.h"
#include "stcp_client.h"

//创建两个连接, 一个使用客户端端口号87和服务器端口号88. 另一个使用客户端端口号89和服务器端口号90.
#define CLIENTPORT1 6087
#define SERVERPORT1 6088
#define CLIENTPORT2 6089
#define SERVERPORT2 6090

//在连接到SIP进程后, 等待1秒, 让服务器启动.
#define STARTDELAY 1
//在发送字符串后, 等待5秒, 然后关闭连接.
#define WAITTIME 5


//这个函数通过在客户和服务器之间创建TCP连接来启动重叠网络层. 它返回TCP套接字描述符, STCP将使用该描述符发送段. 如果TCP连接失败, 返回-1.
int connectToSIP()
{
	return tcp_client_conn_a("127.0.0.1", SIP_PORT);
}

//这个函数断开到本地SIP进程的TCP连接. 
void disconnectToSIP(int sip_conn) 
{
	close(sip_conn);
}

int main() 
{
	//用于丢包率的随机数种子
	srand(time(NULL));

	//连接到SIP进程并获得TCP套接字描述符	
	int sip_conn = connectToSIP();
	if(sip_conn < 0) {
		printf("fail to connect to the local SIP process\n");
		exit(1);
	}

	//初始化stcp客户端
	stcp_client_init(sip_conn);
	sleep(STARTDELAY);

	char hostname[50];
	printf("Enter server name to connect:");
	scanf("%s", hostname);
	int server_nodeID = topology_getNodeIDfromName(hostname);
	if(server_nodeID == -1) {
		printf("host name error!\n");
		exit(1);
	} else {
		printf("connecting to node %d\n", server_nodeID);
	}

	//在端口87上创建STCP客户端套接字, 并连接到STCP服务器端口88
	int sockfd = stcp_client_sock(CLIENTPORT1);
	if(sockfd < 0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd, server_nodeID, SERVERPORT1) < 0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n", CLIENTPORT1, SERVERPORT1);
	
	//在端口89上创建STCP客户端套接字, 并连接到STCP服务器端口90
	int sockfd2 = stcp_client_sock(CLIENTPORT2);
	if(sockfd2 < 0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd2, server_nodeID, SERVERPORT2) < 0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n", CLIENTPORT2, SERVERPORT2);

	//通过第一个连接发送字符串
    char mydata[6] = "hello";
	int i;
	for(i = 0; i < 5; i++) {
      	stcp_client_send(sockfd, mydata, 6);
		printf("send string:%s to connection 1\n", mydata);	
    }
	//通过第二个连接发送字符串
    char mydata2[7] = "byebye";
	for(i = 0; i < 5; i++) {
      	stcp_client_send(sockfd2, mydata2, 7);
		printf("send string:%s to connection 2\n", mydata2);	
    }

	//等待一段时间, 然后关闭连接
	sleep(WAITTIME);

	if(stcp_client_disconnect(sockfd) < 0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd) < 0) {
		printf("fail to close stcp client\n");
		exit(1);
	}
	if(stcp_client_disconnect(sockfd2) < 0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd2) < 0) {
		printf("fail to close stcp client\n");
		exit(1);
	}

	//断开与SIP进程之间的连接
	disconnectToSIP(sip_conn);
}
