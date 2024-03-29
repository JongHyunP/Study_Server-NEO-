#include <WinSock2.h>
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include "PACKET_HEADER_CATCH_MIND.h"

//소켓프로그래밍책 기준.

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

uintptr_t _beginthreadex(
	void *security,
	unsigned stack_size,
	DWORD WINAPI(LPVOID arg),
	void *arglist,
	unsigned initflag,
	unsigned *thrdaddr
);

void _endthreadex(unsigned retval);


DWORD WINAPI WorkerThread(LPVOID arg);
void err_quit(const char *msg);
void err_display(const char *szMsg);


int main(int argc, char *argv[])
{
	int retval;

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) //라이브러리 초기화, 2.2버전 , 데이터 넣고있, 0이아니면 리턴
	{
		return 1;
	}

	HANDLE hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (hcp == NULL) return 1;

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	HANDLE hThread;

	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++)
	{
		hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, hcp, 0, NULL);
		if (hThread == NULL) return 1;
		CloseHandle(hThread);
	}

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

	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	DWORD recvbytes;
	DWORD flags;

	while (1)
	{
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR *)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			break;
		}
		printf("\n[TCP 서버] 클라이언트 접속 : IP 주소 = %s , 포트번호 = %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		CreateIoCompletionPort((HANDLE)client_sock, hcp, client_sock, 0);

		SOCKETINFO *ptr = new SOCKETINFO;
		if (ptr == NULL)
		{
			break;
		}
		ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
		ptr->sock = client_sock;
		ptr->recvbytes = ptr->sendbytes = 0;
		ptr->wsabuf.buf = ptr->buf;
		ptr->wsabuf.len = BUFSIZE;

		//비동기 입출력 시작
		flags = 0;
		retval = WSARecv(client_sock, &ptr->wsabuf, 1, &recvbytes, &flags, &ptr->overlapped, NULL);
		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSARecv()");
			}
			continue;
		}

	}

	WSACleanup();

	return 0;

}

DWORD WINAPI WorkerThread(LPVOID arg)
{
	int retval;
	HANDLE hcp = (HANDLE)arg;

	while (1)
	{
		DWORD cbTransferred;
		SOCKET client_sock;
		SOCKETINFO *ptr;

		retval = GetQueuedCompletionStatus(hcp, &cbTransferred, (LPDWORD)&client_sock, (LPOVERLAPPED*)&ptr, INFINITE);

		//클라이언트 정보 얻기

		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);

		//비동기 입출력 결과 확인
		if (retval == 0 || cbTransferred == 0)
		{
			if (retval == 0)
			{
				DWORD temp1, temp2;
				WSAGetOverlappedResult(ptr->sock, &ptr->overlapped, &temp1, FALSE, &temp2);
				err_display("WSAGetOverlappedResult()");
			}
			closesocket(ptr->sock);
			printf("[TCP서버] 클라이언트 종료 : IP 주소 = %s , 포트번호 = %d \n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
			delete ptr;
			continue;

		}

		//데이터 전송량 갱신
		if (ptr->recvbytes == 0)
		{
			ptr->recvbytes = cbTransferred;
			ptr->sendbytes = 0;

			//받은데이터 출력
			ptr->buf[ptr->recvbytes] = '\0';
			printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), ptr->buf);
		}
		else
		{
			ptr->sendbytes += cbTransferred;
		}

		if (ptr->recvbytes > ptr->sendbytes)
		{
			//데이터 보내기
			ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
			ptr->wsabuf.buf = ptr->buf + ptr->sendbytes;
			ptr->wsabuf.len = ptr->recvbytes - ptr->sendbytes;

			DWORD sendbytes;
			retval = WSASend(ptr->sock, &ptr->wsabuf, 1, &sendbytes, 0, &ptr->overlapped, NULL);
			if (retval == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					err_display("WSASend()");
				}
				continue;
			}
		}
		else
		{
			ptr->recvbytes = 0;

			//데이터 받기
			ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
			ptr->wsabuf.buf = ptr->buf;
			ptr->wsabuf.len = BUFSIZE;

			DWORD recvbytes;
			DWORD flags = 0;
			retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes, &flags, &ptr->overlapped, NULL);
			if (retval == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					err_display("WSARecv()");
				}
				continue;
			}
		}

	}
	return 0;

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


void err_display(const char* szMsg)
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s]%s\n", szMsg, lpMsgBuf);
	LocalFree(lpMsgBuf);
}