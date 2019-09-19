#include <WinSock2.h> // ������ ���Ͽ� ���
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT 9000
#define BUFSIZE 512

//���� �޽��� �����ϰ� ��������.

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

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) //���̺귯�� �ʱ�ȭ, 2.2���� , ������ �ְ���, 0�̾ƴϸ� ����
	{
		return 1;
	}

	// socket()

	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0); // ���ӿ� ���� , ���Ӵ��� ������ (4, txp,
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
	//----------------------��������� �ʱ�ȭ


	SOCKET client_sock; // ��ſ����
	SOCKADDR_IN clientaddr;

	int addrlen;
	char buf[BUFSIZE + 1];

	while (1)
	{
		//accept
		ACCEPT_AGAIN:
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen); // ��ŷ�Լ�(�Է´��) accept
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			break;
		}

		//������ Ŭ���̾�Ʈ ���� ���
		printf("\n[TCP ����] Ŭ���̾�Ʈ ���� : IP �ּ� = %s , ��Ʈ��ȣ = %d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));

		// Ŭ���̾�Ʈ�� ������ ���
		while (1)
		{
			//������ �ޱ�
			retval = recv(client_sock, buf, BUFSIZE, 0); // ��ŷ �Լ� recv
			if (retval == SOCKET_ERROR) // ����������
			{
				err_display("recv()");
				break;
			}
			else if (retval == 0) //�Ϲ� ����
			{
				break;
			}
			//0�̰ų� ������ �ƴϸ� 

			//���������� ���

			buf[retval] = '\0';
			printf("[TCP/%s:%d] %s\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port), buf);

			//������ ������
			retval = send(client_sock, buf, retval, 0);
			if (retval == SOCKET_ERROR)
			{
				err_display("send()");
				break;
			}
		}

		//closesocket()

		closesocket(client_sock);
		printf("[TCP����] Ŭ���̾�Ʈ ���� : IP �ּ� = %s , ��Ʈ��ȣ = %d \n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
	}

	closesocket(listen_sock);

	WSACleanup();
	return 0;

}