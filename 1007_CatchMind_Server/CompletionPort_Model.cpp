// Server.cpp : Defines the entry point for the console application.
//
#include "StdAfx.h"



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

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ProcessSocketMessage(HWND, UINT, WPARAM, LPARAM);
bool ProcessPacket(SOCKET sock, USER_INFO* pUser, char* szBuf, int& len);
void err_display(int errcode);
void err_display(char* szMsg);
//////////////////////////////////////////////////////////////////////////
//	�Լ� ���� ����

UINT WINAPI CompletionThread(LPVOID pCompletionPortIO);


//////////////////////////////////////////////////////////////////////////
//	main() �Լ� ����

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	HANDLE  hCompletionPort;

	SYSTEM_INFO systemInfo;
	SOCKADDR_IN servAddr;

	LPPER_IO_DATA     perIoData;
	LPPER_HANDLE_DATA perHandleData;

	SOCKET hServSock;
	DWORD  dwRecvBytes;
	DWORD  i, dwFlags;

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

	HWND hWnd = CreateWindow("WSAAsyncSelectServer", "TCP ����", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, NULL, (HMENU)NULL, NULL, NULL);
	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	//if (argc != 2)
	//{
	//	printf("Usage : %s <port>\n", argv[0]);
	//	exit(1);
	//}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{	// Load Winsock 2.2 DLL
		ErrorHandling("WSAStartup() error!");
	}

	// 1. Completion Port ����
	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&systemInfo);

	// 2. �����带 CPU ������ŭ ����
	for (i = 0; i < systemInfo.dwNumberOfProcessors; i++)
	{
		_beginthreadex(NULL, 0, CompletionThread, (LPVOID)hCompletionPort, 0, NULL);
	}

	hServSock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (hServSock == INVALID_SOCKET)
	{
		ErrorHandling("WSASocket() error!");
	}

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(atoi(argv[1]));

	if (bind(hServSock, (SOCKADDR *)&servAddr, sizeof(servAddr))
		== SOCKET_ERROR)
	{
		ErrorHandling("bind() error!");
	}

	if (listen(hServSock, 5) == SOCKET_ERROR)
	{
		ErrorHandling("listen() error!");
	}

	while (TRUE)
	{
		SOCKET      hClntSock;
		SOCKADDR_IN clntAddr;
		int         nAddrLen = sizeof(clntAddr);

		hClntSock = accept(hServSock, (SOCKADDR *)&clntAddr,
			&nAddrLen);
		if (hClntSock == INVALID_SOCKET)
		{
			ErrorHandling("accept() error!");
		}

		// ����� Ŭ���̾�Ʈ�� ���� �ڵ� ������ �ּ� ������ ����
		perHandleData = new PER_HANDLE_DATA;
		perHandleData->hClntSock = hClntSock;
		memcpy(&(perHandleData->clntAddr), &clntAddr, nAddrLen);

		// 3. Overlapped ���ϰ� Completion Port �� ����
		CreateIoCompletionPort((HANDLE)hClntSock, hCompletionPort, (DWORD)perHandleData, 0);

		// Ŭ���̾�Ʈ�� ���� ���۸� ����, OVERLAPPED ���� �ʱ�ȭ
		perIoData = new PER_IO_DATA;
		memset(&(perIoData->overlapped), 0, sizeof(OVERLAPPED));
		perIoData->wsaBuf.len = BUFSIZE;
		perIoData->wsaBuf.buf = perIoData->buffer;
		dwFlags = 0;

		// 4. ��ø�� ����Ÿ �Է�
		WSARecv(perHandleData->hClntSock,		// ����Ÿ �Է� ����
			&(perIoData->wsaBuf),				// ����Ÿ �Է� ���� ������
			1,									// ����Ÿ �Է� ������ ��
			&dwRecvBytes,
			&dwFlags,
			&(perIoData->overlapped),			// OVERLAPPED ���� ������
			NULL
		);
	}

	return 0;
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

//////////////////////////////////////////////////////////////////////////
//	CompletionThread() �Լ� ����

UINT WINAPI CompletionThread(LPVOID pCompletionPortIO)
{
	HANDLE hCompletionPort = (HANDLE)pCompletionPortIO;
	DWORD  dwBytesTransferred;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_DATA     perIoData;
	DWORD  dwFlags;

	while (TRUE)
	{
		// 5. ��.����� �Ϸ�� ������ ���� ����
		GetQueuedCompletionStatus(hCompletionPort,		// Completion Port
			&dwBytesTransferred,						// ���۵� ����Ʈ ��
			(LPDWORD)&perHandleData,
			(LPOVERLAPPED *)&perIoData,
			INFINITE
		);

		if (dwBytesTransferred == 0)		// EOF ���� �� 
		{
			closesocket(perHandleData->hClntSock);
			printf("[TCP����] Ŭ���̾�Ʈ ���� : IP �ּ� = %s , ��Ʈ��ȣ = %d \n", inet_ntoa(perHandleData->clntAddr.sin_addr), ntohs(perHandleData->clntAddr.sin_port));
			delete perHandleData;
			delete perIoData;

			continue;
		}

		// 6. �޼���. Ŭ���̾�Ʈ�� ����
		perIoData->wsaBuf.len = dwBytesTransferred;
		WSASend(perHandleData->hClntSock, &(perIoData->wsaBuf), 1, NULL, 0, NULL, NULL);

		// RECEIVE AGAIN
		memset(&(perIoData->overlapped), 0, sizeof(OVERLAPPED));
		perIoData->wsaBuf.len = BUFSIZE;
		perIoData->wsaBuf.buf = perIoData->buffer;

		dwFlags = 0;
		WSARecv(perHandleData->hClntSock,
			&(perIoData->wsaBuf),
			1,
			NULL,
			&dwFlags,
			&(perIoData->overlapped),
			NULL
		);
	}

	return 0;
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
	case FD_ACCEPT: //Ŭ�� �����Ҷ�
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
		printf("[TCP ����] Ŭ���̾�Ʈ ���� : IP �ּ� = %s , ��Ʈ��ȣ = %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		retval = WSAAsyncSelect(client_sock, hWnd, WM_SOCKET, FD_READ | FD_CLOSE);
		if (retval == SOCKET_ERROR)
		{
			cout << "err on WSAAsyncSelect!!" << endl;
		}

		//���������� ������ش�
		USER_INFO* pInfo = new USER_INFO();
		pInfo->index = g_iIndex++; //���� ��ȣ
		pInfo->len = 0;				// ����?
		pInfo->x = rand() % 600;	//X��ġ
		pInfo->y = rand() % 400;    //Y��ġ

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

		g_mapUser.insert(make_pair(client_sock, pInfo)); // ���� �߰�

		for (int i = 0; i < 20; i++)
			cout << pInfo->ranarr[i] << " ";

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
		for (auto iter = g_mapUser.begin(); iter != g_mapUser.end(); iter++, i++)
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

		//ī�� ���� ����
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

		retval = recv(wParam, szBuf, BUFSIZE, 0);
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
			if (!ProcessPacket(wParam, pUser, szBuf, retval))
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
bool ProcessPacket(SOCKET sock, USER_INFO* pUser, char* szBuf, int& len)
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

	switch (header.wIndex) // ����� ������ ����
	{
	case PACKET_INDEX_SEND_POS: //��ġ ������ ������
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

	memcpy(&pUser->szBuf, &pUser->szBuf[header.wLen], pUser->len - header.wLen);
	pUser->len -= header.wLen;

	return true;
}

void err_display(int errcode)
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[����]%s", lpMsgBuf);
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