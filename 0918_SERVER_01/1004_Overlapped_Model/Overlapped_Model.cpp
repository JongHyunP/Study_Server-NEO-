#pragma comment(lib,"ws2_32")
#include <WinSock2.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE 512

struct SOCKETINFO
{
	WSAOVERLAPPED overlapped;
	SOCKET sock;
	char buf[BUFSIZE + 1];
	int recvbytes;
	int sendbytes;
	WSABUF wsabuf;
};

SOCKET client_sock;
HANDLE hReadEvent, hWriteEvent;

DWORD WINAPI WorkerThread(LPVOID arg);
void CALLBACK CompletionRoutine(
	DWORD dwError, DWORD cbTransferred,
	LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags
);


//오류 출력함수 

void err_quit(char *msg);
void err_display(char *msg);
void err_display(int errcode);

int main(int argc, char *argv[])
{
	int retval;

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) //라이브러리 초기화, 2.2버전 , 데이터 넣고있, 0이아니면 리턴
	{
		return 1;
	}

	// socket()

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0); // 접속용 소켓 , 접속대기용 소켓임 (4, txp,
	if (listen_sock == INVALID_SOCKET)
	{
		err_quit("socket()");
	}

	// bind 
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
	//----------------------여기까지가 초기화

	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (hReadEvent == NULL) return 1;
	hWriteEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (hWriteEvent == NULL) return 1;

	HANDLE hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
	if (hThread == NULL) return 1;
	CloseHandle(hThread);

	while (1)
	{
		WaitForSingleObject(hReadEvent, INFINITE);
		//accept()
		client_sock = accept(listen_sock, NULL, NULL);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			break;
		}
		SetEvent(hWriteEvent);
	}

	WSACleanup();

	return 0;
}


DWORD WINAPI WorkerThread(LPVOID arg)
{
	int retval;

	while (1)
	{
		while (1)
		{
			DWORD result = WaitForSingleObjectEx(hWriteEvent, INFINITE, TRUE);
			if (result == WAIT_OBJECT_0) break;
			if (result == WAIT_IO_COMPLETION) return 1;
		}

		SOCKADDR_IN clientaddr;

		int addrlen = sizeof(clientaddr);
		getpeername(client_sock, (SOCKADDR*)&clientaddr, &addrlen);
		printf("\n[TCP 서버] 클라이언트 접속 : IP 주소 = %s , 포트번호 = %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		//소켓 정보 구조체 할당과 초기화

		SOCKETINFO *ptr = new SOCKETINFO;
		if (ptr == NULL)
		{
			printf("[오류] 메모리가 부족합니다! \n");
			return 1;
		}
		ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
		ptr->sock = client_sock;
		SetEvent(hReadEvent);
		ptr->recvbytes = ptr->sendbytes = 0;
		ptr->wsabuf.buf = ptr->buf;
		ptr->wsabuf.len = BUFSIZE;

		//비동기 입출력 시작
		DWORD recvbytes;
		DWORD flags = 0;
		retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes, &flags, &ptr->overlapped, CompletionRoutine);
		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSARecv()");
				return 1;
			}
		}
	}
	return 0;
}

void CALLBACK CompletionRoutine(
	DWORD dwError, DWORD cbTransferred,
	LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags
)
{
	int retval;
	//클라이언트 정보얻기

	SOCKETINFO *ptr = (SOCKETINFO*)lpOverlapped;
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);

	if (dwError != 0 || cbTransferred == 0)
	{
		if (dwError != 0) err_display(dwError);
		closesocket(ptr->sock);
		printf("[TCP서버] 클라이언트 종료 : IP 주소 = %s , 포트번호 = %d \n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
		delete ptr;
		return;
	}

	if (ptr->recvbytes == 0)
	{
		ptr->recvbytes = cbTransferred;
		ptr->sendbytes = 0;
		
		//받은데이터 출력
		ptr->buf[ptr->recvbytes] = '\0';
		printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), ptr->buf);
	}
	else {
		ptr->sendbytes += cbTransferred;
	}

	if (ptr->recvbytes > ptr->sendbytes)
	{
		//데이터 보내기
		ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
		ptr->wsabuf.buf = ptr->buf + ptr->sendbytes;
		ptr->wsabuf.len = ptr->recvbytes - ptr->sendbytes;

		DWORD sendbytes;
		retval = WSASend(ptr->sock, &ptr->wsabuf, 1, &sendbytes, 0, &ptr->overlapped, CompletionRoutine);
		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSASend()");
				return;
			}
		}
	}
	else {
		ptr->recvbytes = 0;

		ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
		ptr->wsabuf.buf = ptr->buf;
		ptr->wsabuf.len = BUFSIZE;

		DWORD recvbytes;
		DWORD flags = 0;
		retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes, &flags, &ptr->overlapped, CompletionRoutine);
		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSARecv()");
				return;
			}
		}
	}
}

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