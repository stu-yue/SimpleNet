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
	segPtr->header.checksum = checksum(segPtr);

	int n;
	if (send(connection, BEGIN_SIGN, 2, 0) <= 0) {
		printf("CONNECTION ERROR: [SIP] CAN'T [SEND] [BEGIN]\n");
		return -1;
	}
	if (send(connection, segPtr, sizeof(seg_t), 0) <= 0) {
		printf("CONNECTION ERROR: [SIP] CAN'T [SEND] [SEGMENT]\n");
		return -1;
	}
	if (send(connection, END_SIGN, 2, 0) <= 0) {
		printf("CONNECTION ERROR: [SIP] CAN'T [SEND] [END]\n");
		return -1;
	}
	printf("SEG[%s] SEND: %d BYTES [PORT: %d | SEQ: %3d]\n", 
		SEG_TYPE[segPtr->header.type], segPtr->header.length, segPtr->header.src_port, segPtr->header.seq_num);
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
		printf("CONNECTION ERROR: [SIP] CAN'T [RECV] [BEGIN]\n");
		return -1;
	}

	// 接收seg_t
	if (n = recv(connection, segPtr, sizeof(seg_t), 0) <= 0) {
		printf("CONNECTION ERROR: [SIP] CAN'T [RECV] [SEGMENT]\n");
		return -1;
	}

	// 确保以!#结尾
	if ((n = recv(connection, &sign, 2, 0)) == 2) {
		if (strcmp(sign, END_SIGN) != 0)
			return 0;
	} else if (n <= 0) {
		printf("CONNECTION ERROR: [SIP] CAN'T [RECV] [END]\n");
		return -1;
	}

	if (seglost(segPtr) || checkchecksum(segPtr) == -1) {
		return 0;
	} else {
		return 1;
	}
}


int seglost(seg_t* segPtr) 
{
	int random = rand() % 100;
	if(random < PKT_LOSS_RATE * 100) {
		if (rand() % 2 == 0) {
			// 50%概率丢失段
			printf("SEG[%s] LOST: %d BYTES [PORT: %d | SEQ: %3d]\n", 
				SEG_TYPE[segPtr->header.type], segPtr->header.length, segPtr->header.src_port, segPtr->header.seq_num);
			return 1;
		} else {
			// 50%概率校验和错误
			printf("SEG[%s] FLIP: %d BYTES [PORT: %d | SEQ: %3d] ", 
				SEG_TYPE[segPtr->header.type], segPtr->header.length, segPtr->header.src_port, segPtr->header.seq_num);
			//获取数据长度
			int len = sizeof(stcp_hdr_t) + segPtr->header.length;
			//获取要反转的随机位
			int errorbit = rand() % (len * 8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit / 8;
			*temp = *temp ^ ( 1 << (errorbit % 8));
			return 0;
		}
	}
	printf("SEG[%s] RECV: %d BYTES [PORT: %d | SEQ: %3d] ", 
		SEG_TYPE[segPtr->header.type], segPtr->header.length, segPtr->header.src_port, segPtr->header.seq_num);
	return 0;
}


unsigned short checksum(seg_t* segment)
{
	segment->header.checksum = 0;
	// 获取数据长度
	int len = (sizeof(stcp_hdr_t) + segment->header.length) / 2;
	// 补充数据长度为偶数
	if (len % 2 == 1) {
		segment->data[segment->header.length] = 0;
		len++;
	}
	unsigned long sum;
	unsigned short *buf = (unsigned short*)segment;
	for (sum = 0; len > 0; len--) {
		sum += *buf++;
	}
	while (sum >> 16)
		sum = (sum >> 16) + (sum & 0xffff);
	return ~sum;
}


int checkchecksum(seg_t* segment)
{
	int len = (sizeof(stcp_hdr_t) + segment->header.length) / 2;
	if (len % 2 == 1) {
		segment->data[segment->header.length] = 0;
		len++;
	}
	unsigned long sum;
	unsigned short *buf = (unsigned short*)segment;
	for (sum = 0; len > 0; len--)
		sum += *buf++;
	while (sum >> 16)
		sum = (sum >> 16) + (sum &0xffff);
	sum = ~sum;
	if ((sum & 0xffff) == 0) {
		// 检验和正确
		printf(" [CHECKSUM: OK]\n");
		return 1;
	} else {
		// 检验和错误
		printf(" [CHECKSUM: ERROR]\n");
		return -1;
	}
}