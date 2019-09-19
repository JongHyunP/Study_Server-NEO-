#include <WinSock2.h> // ������ ���Ͽ� ���
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE 512
#define WM_SOCKET (WM_USER+1)

struct SOCKETINFO 
{
	SOCKET sock;
	char buf[BUFSIZE + 1];
	int recvbytes;
	int sendbytes;
	BOOL recvdelayed;
	SOCKETINFO *next;
};

SOCKETINFO *SocketInfoList;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ProcessSocketMessage(HWND, UINT, WPARAM, LPARAM);
BOOL AddSocketInfo(SOCKET sock);
SOCKETINFO *GetSocketInfo(SOCKET sock);
void RemoveSocketInfo(SOCKET sock);

void err_quit(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

void err_display(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

void err_display(int errcode) 
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[����] %s", (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

int main(int argc, char *argv[])
{
	int retval;

	WNDCLASS WndClass;
	WndClass.cbClsExtra = 0; //��� 0��
	WndClass.cbWndExtra = 0; //��� 0��
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	//���ȭ�� �귯��,�̸������ִ� �귯���� (F12 ���� �ö󰡼� ����������)
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	//Ŀ�� ����
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	//������ ����
	WndClass.hInstance = NULL;
	WndClass.lpfnWndProc = WndProc;
	//���ν����� �Լ� ������
	WndClass.lpszClassName = "MyWndClass";
	WndClass.lpszMenuName = NULL;
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	//âũ��

	if (!RegisterClass(&WndClass)) return 1;

	RegisterClass(&WndClass); //��Ʈ��Ʈ �� ����, ���� ������ �ɼ����� Ŀ���� Ŭ������ �����.

	HWND hWnd = CreateWindow("MyWndClass", "TCP ����", WS_OVERLAPPEDWINDOW, 0, 0, 600, 200, NULL, NULL,NULL, NULL);
	if (hWnd == NULL) return 1;

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) //���̺귯�� �ʱ�ȭ, 2.2���� , ������ �ְ���, 0�̾ƴϸ� ����
	{
		return 1;
	}

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0); // ���ӿ� ���� , ���Ӵ��� ������ (4, txp,
	if (listen_sock == INVALID_SOCKET)
	{
		err_quit("socket()");
	}

	SOCKADDR_IN serveraddr;

	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // �ƹ��� �ްڴ�. ip ���������� �𸣴ϱ�
	serveraddr.sin_port = htons(SERVERPORT); // 9000�� ��Ʈ ���ٴ�
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		err_quit("bind()");
	}

	retval = listen(listen_sock, SOMAXCONN);

	if (retval == SOCKET_ERROR)
	{
		err_quit("listen()");
	}

	//WSAAsyncSelect()

	retval = WSAAsyncSelect(listen_sock, hWnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE);
	if (retval == SOCKET_ERROR)
	{
		err_quit("WSAAsyncSelect()");
	}

	MSG msg;
	while (GetMessage(&msg,0,0,0)>0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	//���� ����

	WSACleanup();
	return msg.wParam;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) // W�Ķ���� L�Ķ���ʹ� �޽��� �̿��� �ΰ��������� �ʿ��Ҷ�
{

	switch (uMsg) //�ַ� �޽����� ������ �ϴµ� �ڵ��� ��.
	{
	case WM_SOCKET:
		ProcessSocketMessage(hWnd, uMsg, wParam, lParam);
		return 0;
	case WM_DESTROY: //WM = ������޽��� ���Ӹ�
		PostQuitMessage(0); //����޽���
		return 0;//ó���޴�.
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);

}

void ProcessSocketMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SOCKETINFO *ptr;
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	int retval;

	if (WSAGETSELECTERROR(lParam))
	{
		err_display(WSAGETSELECTERROR(lParam));
		RemoveSocketInfo(wParam);
		return;
	}

	switch (WSAGETSELECTEVENT(lParam))
	{
	case FD_ACCEPT:
		addrlen = sizeof(clientaddr);
		client_sock = accept(wParam, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			return;
		}

		printf("\n[TCP ����] Ŭ���̾�Ʈ ���� : IP �ּ� = %s , ��Ʈ��ȣ = %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		AddSocketInfo(client_sock);

		retval = WSAAsyncSelect(client_sock, hWnd, WM_SOCKET, FD_READ|FD_WRITE| FD_CLOSE);

		if (retval == SOCKET_ERROR)
		{
			err_display("WSAAsyncSelect()");
			RemoveSocketInfo(client_sock);
		}
		break;

	case FD_READ:
		ptr = GetSocketInfo(wParam);
		if (ptr->recvbytes > 0)
		{
			ptr->recvdelayed = TRUE;
			return;
		}
		retval = recv(ptr->sock, ptr->buf, BUFSIZE, 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("recv()");
			RemoveSocketInfo(wParam);
			return;
		}
		ptr->recvbytes = retval;

		ptr->buf[retval] = '\0';
		getpeername(wParam, (SOCKADDR*)&clientaddr, &addrlen);
		printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), ptr->buf);
	case FD_WRITE:
		ptr = GetSocketInfo(wParam);
		if (ptr->recvbytes <= ptr->sendbytes)
		{
			return;
		}

		//������ ������
		retval = send(ptr->sock, ptr->buf + ptr->sendbytes, ptr->recvbytes - ptr->sendbytes, 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			RemoveSocketInfo(wParam);
			return;
		}
		ptr->sendbytes += retval;
		//���������͸� ��� ���´��� üũ

		if (ptr->recvbytes == ptr->sendbytes)
		{
			ptr->recvbytes = ptr->sendbytes = 0;
			if (ptr->recvdelayed)
			{
				ptr->recvdelayed = FALSE;
				PostMessage(hWnd, WM_SOCKET, wParam, FD_READ);
			}
		}
		break;
	case FD_CLOSE:
		RemoveSocketInfo(wParam);
		break;
	}
}

BOOL AddSocketInfo(SOCKET sock)
{
	SOCKETINFO *ptr = new SOCKETINFO;

	if (ptr == NULL)
	{
		printf("[����] �޸𸮰� �����մϴ� .");
		return FALSE;
	}

	ptr->sock = sock;
	ptr->recvbytes = 0;
	ptr->sendbytes = 0;
	ptr->recvdelayed = FALSE;
	ptr->next = SocketInfoList;

	SocketInfoList = ptr;

	return TRUE;
}



SOCKETINFO *GetSocketInfo(SOCKET sock) 
{
	SOCKETINFO *ptr = SocketInfoList;
	while (ptr)
	{
		if (ptr->sock == sock)
		{
			return ptr;
		}
		ptr = ptr->next;
	}
	return NULL;
}
void RemoveSocketInfo(SOCKET sock)
{
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(sock, (SOCKADDR*)&clientaddr, &addrlen);
	printf("[TCP����] Ŭ���̾�Ʈ ���� : IP �ּ� = %s , ��Ʈ��ȣ = %d \n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

	SOCKETINFO *curr = SocketInfoList;
	SOCKETINFO *prev = NULL;

	while (curr)
	{
		if (curr->sock == sock)
		{
			if (prev)
			{
				prev->next = curr->next;
			}
			else
			{
				SocketInfoList = curr->next;
			}
			closesocket(curr->sock);
			delete curr;
			return;
		}
		prev = curr;
		curr = curr->next;
	}
}


