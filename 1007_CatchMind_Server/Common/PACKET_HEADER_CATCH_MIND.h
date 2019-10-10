#pragma once
#include <Windows.h>
#pragma pack(1)

enum PACKET_INDEX
{
	PACKET_INDEX_LOGIN_RET = 1,
	PACKET_INDEX_USER_DATA,
	PACKET_INDEX_SEND_POS
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

struct LOGIN_DATA
{
	int iIndex;
	char cID[10];
	int ipassword;

};

struct LOBBY_DATA
{
	int iIndex;
	int iRoom[20];
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


struct PACKET_SEND_POS
{
	PACKET_HEADER header;
	USER_DATA data;
};

struct PACKET_SEND_LOGIN
{
	PACKET_HEADER header;
	LOGIN_DATA data;
};

struct PACKET_SEND_LOBBY //로비 데이터용
{
	PACKET_HEADER header;
	LOBBY_DATA data;
};



#pragma pack()