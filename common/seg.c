/**
 * @file seg.c
 * @brief 这个文件包含用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的实现.
 * @date 2023-02-18
 */

#include "seg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>


const char* BEGIN_SIGN = "!&";
const char* END_SIGN = "!#";
const char* SEG_TYPE[6] = {"SYN", "SYNACK", "FIN", "FINACK", "DATA", "DATAACK"};


int sip_sendseg(int connection, seg_t* segPtr)
{
	int n;
	if (send(connection, BEGIN_SIGN, 2, 0) <= 0) {
		printf("SIP SEND: SEND BEGIN ERROR\n");
		return -1;
	}
	if (send(connection, segPtr, sizeof(seg_t), 0) <= 0) {
		printf("SIP SEND: SEND SEGMENT ERROR\n");
		return -1;
	}
	if (send(connection, END_SIGN, 2, 0) <= 0) {
		printf("SIP SEND: SEND END ERROR\n");
		return -1;
	}
	// fprintf(stdout, "Seg[%s] send, total %d bytes\n", SEG_TYPE[segPtr->header.type], n);
	return 1;
}


int sip_recvseg(int connection, seg_t* segPtr)
{
	int n, state;
	char sign[3] = {0, 0, 0}, ch;

	// 确保以!&开始
	state = 0;
	while ((n = recv(connection, &ch, 1, 0)) == 1) {
		switch (state){
			case 0: state = (ch == '!' ? 1 : 0);
				break;
			case 1: state = (ch == '&' ? 2 : 0);
				break; 
		}
		if (state == 2) break;
	}
	if (n == 0)
		return 0;
	if (n < 0) {
		printf("SIP RECV: RECV BEGIN ERROR\n");
		return -1;
	}

	// 接收seg_t
	if (n = recv(connection, segPtr, sizeof(seg_t), 0) <= 0) {
		printf("SIP RECV: RECV SEGMENT ERROR\n");
		return -1;
	}

	// 确保以!#结尾
	if ((n = recv(connection, &sign, 2, 0)) == 2) {
		if (strcmp(sign, END_SIGN) != 0)
			return 0;
	} else if (n <= 0) {
		printf("SIP RECV: RECV END ERROR\n");
		return -1;
	}

	if (seglost(segPtr)) {
		return 0;
	} else {
		return 1;
	}
}


int seglost(seg_t* segPtr) {
	int random = rand() % 100;
	if(random < PKT_LOSS_RATE * 100) {
		printf("SEG[%s] LOST: %d BYTES (FROM %d)\n", SEG_TYPE[segPtr->header.type], segPtr->header.length, segPtr->header.src_port);
		return 1;
	} else {
		printf("SEG[%s] RECV: %d BYTES (FROM %d)\n", SEG_TYPE[segPtr->header.type], segPtr->header.length, segPtr->header.src_port);
		return 0;
	}
}