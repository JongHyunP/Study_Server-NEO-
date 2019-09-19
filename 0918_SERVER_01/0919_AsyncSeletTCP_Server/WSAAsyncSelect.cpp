#include <WinSock2.h> // 윈도우 소켓에 사용
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
	printf("[오류] %s", (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

int main(int argc, char *argv[])
{
	int retval;

	WNDCLASS WndClass;
	WndClass.cbClsExtra = 0; //통상 0임
	WndClass.cbWndExtra = 0; //통상 0임
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	//배경화면 브러쉬,미리쓸수있는 브러쉬들 (F12 참조 올라가서 색볼수있음)
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	//커서 변경
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	//아이콘 변경
	WndClass.hInstance = NULL;
	WndClass.lpfnWndProc = WndProc;
	//프로시저의 함수 포인터
	WndClass.lpszClassName = "MyWndClass";
	WndClass.lpszMenuName = NULL;
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	//창크기

	if (!RegisterClass(&WndClass)) return 1;

	RegisterClass(&WndClass); //스트럭트 로 전달, 위에 설정한 옵션으로 커스텀 클래스를 만든다.

	HWND hWnd = CreateWindow("MyWndClass", "TCP 서버", WS_OVERLAPPEDWINDOW, 0, 0, 600, 200, NULL, NULL,NULL, NULL);
	if (hWnd == NULL) return 1;

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) //라이브러리 초기화, 2.2버전 , 데이터 넣고있, 0이아니면 리턴
	{
		return 1;
	}

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0); // 접속용 소켓 , 접속대기용 소켓임 (4, txp,
	if (listen_sock == INVALID_SOCKET)
	{
		err_quit("socket()");
	}

	SOCKADDR_IN serveraddr;

	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // 아무나 받겠다. ip 누가오는지 모르니까
	serveraddr.sin_port = htons(SERVERPORT); // 9000번 포트 쓰겟다
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

	//윈속 종료

	WSACleanup();
	return msg.wParam;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) // W파라미터 L파라미터는 메시지 이외의 부가정보들이 필요할때
{

	switch (uMsg) //주로 메시지를 재정의 하는데 코딩을 함.
	{
	case WM_SOCKET:
		ProcessSocketMessage(hWnd, uMsg, wParam, lParam);
		return 0;
	case WM_DESTROY: //WM = 윈도우메시지 줄임말
		PostQuitMessage(0); //종료메시지
		return 0;//처리햇다.
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

		printf("\n[TCP 서버] 클라이언트 접속 : IP 주소 = %s , 포트번호 = %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

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

		//데이터 보내기
		retval = send(ptr->sock, ptr->buf + ptr->sendbytes, ptr->recvbytes - ptr->sendbytes, 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			RemoveSocketInfo(wParam);
			return;
		}
		ptr->sendbytes += retval;
		//받은데이터를 모두 보냈는지 체크

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
		printf("[오류] 메모리가 부족합니다 .");
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
	printf("[TCP서버] 클라이언트 종료 : IP 주소 = %s , 포트번호 = %d \n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

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


