#pragma once
#include <Windows.h>
#pragma pack(1)

enum PACKET_INDEX
{
	PACKET_INDEX_LOGIN_RET = 1,
	PACKET_INDEX_USER_DATA,
	PACKET_INDEX_SEND_POS,
	PACKET_INDEX_CARD_DATA
};

struct PACKET_HEADER
{
	WORD wIndex;
	WORD wLen;
};

struct USER_DATA
{
	int iIndex;
	WORD wX;
	WORD wY;
	int wArr[20];
};

struct CARD_DATA
{
	int iIndex;
	int wArr[20]; // ��ȣ ���¿뵵
};

struct PACKET_LOGIN_RET
{
	PACKET_HEADER header;
	int iIndex;
};

struct PACKET_USER_DATA
{
	PACKET_HEADER header;
	WORD wCount;
	USER_DATA data[20];
};

struct PACKET_CARD_DATA
{
	PACKET_HEADER header;
	WORD wCount;
	CARD_DATA data[20];
};


struct PACKET_SEND_POS
{
	PACKET_HEADER header;
	USER_DATA data;
};

struct PACKET_SEND_CARDINFO
{
	PACKET_HEADER header;
	CARD_DATA data;
};


#pragma pack()