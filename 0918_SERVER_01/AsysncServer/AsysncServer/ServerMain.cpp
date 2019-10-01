#include <WinSock2.h>
#include <Windows.h>
#include <iostream>
#include <map>
#include <list>
#include "..\..\Common\PACKET_HEADER.h"
using namespace std;

#define BUFSIZE 512
#define WM_SOCKET (WM_USER+1)

class USER_INFO
{
public:
	int index;
	char szBuf[BUFSIZE];
	int len;
	int x;
	int y;
	int ranarr[20];
};

int g_iIndex = 0;
map<SOCKET, USER_INFO*> g_mapUser;

class CARD_INFO
{
public:
	int m_targetUserIndex; //어느 유저한테 뿌릴건지
	int m_randomCardArray[20]; //카드 셔플용 배열
	int len; //길이
};

class LOBBY
{
public:
	list<USER_INFO*> m_UserList;
};



LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ProcessSocketMessage(HWND, UINT, WPARAM, LPARAM);
bool ProcessPacket(SOCKET sock , USER_INFO* pUser, char* szBuf, int& len);

void err_display(int errcode);
void err_display(char* szMsg);

int main(int argc, char* argv[])
{
	int retval;

	WNDCLASS WndClass;

	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hInstance = NULL;
	WndClass.lpfnWndProc = WndProc;
	WndClass.lpszClassName = "WSAAsyncSelectServer";
	WndClass.lpszMenuName = NULL;
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	if (!RegisterClass(&WndClass))
		return -1;

	HWND hWnd = CreateWindow("WSAAsyncSelectServer", "TCP 서버", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, NULL, (HMENU)NULL, NULL, NULL);
	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return -1;

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
	{
		cout << "err on socket" << endl;
		return -1;
	}

	retval = WSAAsyncSelect(listen_sock, hWnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE);
	if (retval == SOCKET_ERROR)
	{
		cout << "err on WSAAsyncSelect" << endl;
		return -1;
	}

	//bind
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(9000);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		cout << "err on bind" << endl;
		return -1;
	}

	//listen
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		cout << "err on listen" << endl;
		return -1;
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SOCKET:
		ProcessSocketMessage(hWnd, uMsg, wParam, lParam);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void ProcessSocketMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen = 0;
	int retval = 0;

	srand(GetTickCount());

	if (WSAGETSELECTERROR(lParam))
	{
		int err_code = WSAGETSELECTERROR(lParam);
		err_display(WSAGETSELECTERROR(lParam));
		return;
	}

	switch (WSAGETSELECTEVENT(lParam))
	{
	case FD_ACCEPT: //클라가 접속할때
	{
		addrlen = sizeof(clientaddr);
		client_sock = accept(wParam, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			int err_code = WSAGetLastError();
			if (err_code != WSAEWOULDBLOCK)
			{
				err_display("err on accept");
			}
			return;
		}
		printf("[TCP 서버] 클라이언트 접속 : IP 주소 = %s , 포트번호 = %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		retval = WSAAsyncSelect(client_sock, hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
		if (retval == SOCKET_ERROR)
		{
			cout << "err on WSAAsyncSelect!!" << endl;
		}

		//유저인포를 만들어준다
		USER_INFO* pInfo = new USER_INFO();
		pInfo->index = g_iIndex++; //유저 번호
		pInfo->len = 0;				// 길이?
		pInfo->x = rand() % 600;	//X위치
		pInfo->y = rand() % 400;    //Y위치

		for (int i = 0; i < 20; i++)
			pInfo->ranarr[i] = i % 10;

		for (int i = 0; i < 100; i++)
		{
			int randA = rand() % 20;
			int randB = rand() % 20;

			int iTemp = pInfo->ranarr[randA];
			pInfo->ranarr[randA] = pInfo->ranarr[randB];
			pInfo->ranarr[randB] = iTemp;
		}

		g_mapUser.insert(make_pair(client_sock , pInfo)); // 유저 추가
		
		for (int i = 0; i < 20; i++)
		cout<<pInfo->ranarr[i]<<" ";

		cout << endl;

		PACKET_LOGIN_RET packet;
		packet.header.wIndex = PACKET_INDEX_LOGIN_RET;
		packet.header.wLen = sizeof(packet);
		packet.iIndex = pInfo->index;
		send(client_sock, (const char*)&packet, packet.header.wLen, 0);

		Sleep(500);

		PACKET_USER_DATA user_packet;
		user_packet.header.wIndex = PACKET_INDEX_USER_DATA;
		user_packet.header.wLen = sizeof(PACKET_HEADER) + sizeof(WORD) + sizeof(INT) + sizeof(USER_DATA) * g_mapUser.size();
		user_packet.wCount = g_mapUser.size();
		int i = 0;
		for (auto iter = g_mapUser.begin(); iter != g_mapUser.end(); iter++ , i++)
		{
			user_packet.data[i].iIndex = iter->second->index;
			user_packet.data[i].wX = iter->second->x;
			user_packet.data[i].wY = iter->second->y;
			for (int j = 0; j < 20; j++)
			{
				user_packet.data[i].wArr[j] = iter->second->ranarr[j];
			//	cout << user_packet.data[i].wArr[j] << " ";
			}
		}

		for (auto iter = g_mapUser.begin(); iter != g_mapUser.end(); iter++, i++)
		{
			send(iter->first, (const char*)&user_packet, user_packet.header.wLen, 0);
		}

		Sleep(500);

		//카드 순서 전달
		PACKET_CARD_DATA card_packet;
		card_packet.header.wIndex = PACKET_INDEX_CARD_DATA;
		card_packet.header.wLen = sizeof(PACKET_HEADER) + sizeof(INT) + sizeof(CARD_DATA) * g_mapUser.size();
		card_packet.wCount = g_mapUser.size();

		int k = 0;
		for (auto iter = g_mapUser.begin(); iter != g_mapUser.end(); iter++, k++)
		{
			card_packet.data[k].iIndex = iter->second->index;
			for (int j = 0; j < 20; j++)
			{
				card_packet.data[k].wArr[j] = iter->second->ranarr[j];
				cout << card_packet.data[k].wArr[j] << " ";
			}
		}

		for (auto iter = g_mapUser.begin(); iter != g_mapUser.end(); iter++, i++)
		{
			send(iter->first, (const char*)&card_packet, card_packet.header.wLen, 0);
		}

	}
	break;
	case FD_READ:
		{
			char szBuf[BUFSIZE];

			retval = recv(wParam, szBuf, BUFSIZE , 0);
			if (retval == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSAEWOULDBLOCK)
				{
					cout << "err on recv!!" << endl;
				}
			}

			USER_INFO* pUser = g_mapUser[wParam];

			while (true)
			{
				if (!ProcessPacket(wParam , pUser, szBuf, retval))
				{
					Sleep(100);
					//SendMessage(hWnd, uMsg, wParam, lParam);
					break;
				}
				else
				{
					if (pUser->len < sizeof(PACKET_HEADER))
						break;
				}
			}
			
		}
		break;
	case FD_CLOSE:
		closesocket(wParam);
		break;
	}
}

bool ProcessPacket(SOCKET sock , USER_INFO* pUser, char* szBuf, int& len)
{
	if (len > 0)
	{
		memcpy(&pUser->szBuf[pUser->len], szBuf, len);
		pUser->len += len;
		len = 0;
	}

	if (pUser->len < sizeof(PACKET_HEADER))
		return false;

	PACKET_HEADER header;
	memcpy(&header, pUser->szBuf, sizeof(header));

	if (pUser->len < header.wLen)
		return false;

	switch (header.wIndex) // 헤더의 종류에 따라
	{
		case PACKET_INDEX_SEND_POS: //위치 정보를 보낸다
		{
			PACKET_SEND_POS packet;
			memcpy(&packet, szBuf, header.wLen);

			g_mapUser[sock]->x = packet.data.wX;
			g_mapUser[sock]->y = packet.data.wY;

			for (auto iter = g_mapUser.begin(); iter != g_mapUser.end(); iter++)
			{
				//if (iter->first == sock)
					//continue;

				send(iter->first, (const char*)&packet, header.wLen, 0);
			}
		}
	break;
	}

	memcpy(&pUser->szBuf , &pUser->szBuf[header.wLen], pUser->len - header.wLen);
	pUser->len -= header.wLen;

	return true;
}

void err_display(int errcode)
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[오류]%s", lpMsgBuf);
	LocalFree(lpMsgBuf);
}

void err_display(char* szMsg)
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s]%s\n", szMsg, lpMsgBuf);
	LocalFree(lpMsgBuf);
}