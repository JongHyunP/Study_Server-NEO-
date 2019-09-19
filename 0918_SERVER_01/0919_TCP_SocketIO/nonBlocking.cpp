#include <WinSock2.h> // 윈도우 소켓에 사용
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE 512

//에러 메시지 세세하게 잡아줘야함.

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

int main()
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

	u_long on = 1;
	retval = ioctlsocket(listen_sock, FIONBIO, &on);
	if (retval == SOCKET_ERROR)
	{
		err_quit("ioctlsockt()");
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


	SOCKET client_sock; // 통신용소켓
	SOCKADDR_IN clientaddr;

	int addrlen;
	char buf[BUFSIZE + 1];

	while (1)
	{
		//accept
		ACCEPT_AGAIN:
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen); // 블럭킹함수(입력대기) accept
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			break;
		}

		//접속한 클라이언트 정보 출력
		printf("\n[TCP 서버] 클라이언트 접속 : IP 주소 = %s , 포트번호 = %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		// 클라이언트와 데이터 통신
		while (1)
		{
			//데이터 받기
			retval = recv(client_sock, buf, BUFSIZE, 0); // 블럭킹 함수 recv
			if (retval == SOCKET_ERROR) // 비정상종료
			{
				err_display("recv()");
				break;
			}
			else if (retval == 0) //일반 종료
			{
				break;
			}
			//0이거나 에러가 아니면 

			//받은데이터 출력

			buf[retval] = '\0';
			printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), buf);

			//데이터 보내기
			retval = send(client_sock, buf, retval, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				break;
			}
		}

		//closesocket()

		closesocket(client_sock);
		printf("[TCP서버] 클라이언트 종료 : IP 주소 = %s , 포트번호 = %d \n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
	}

	closesocket(listen_sock);

	WSACleanup();
	return 0;

}