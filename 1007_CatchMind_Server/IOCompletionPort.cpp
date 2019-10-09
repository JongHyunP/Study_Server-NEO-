#pragma once
#include "IOCompletionPort.h"
#include "StdAfx.h"

unsigned int WINAPI CallWorkerThread(LPVOID p)
{
	IOCompletionPort* pOverlappedEvent = (IOCompletionPort*)p;
	pOverlappedEvent->WorkerThread();

	return 0;
}


IOCompletionPort::IOCompletionPort()
{
	m_iIndex = 0;
	m_bWorkerThread = true;
	m_bAccept = true;
}


IOCompletionPort::~IOCompletionPort()
{
	// winsock�� ����� ������.
	WSACleanup();

	//�� ����� ��ü�� ����
	if (m_pSoketInfo)
	{
		delete[] m_pSoketInfo;
		m_pSoketInfo = NULL;
	}

	if (m_pWorkerHandle)
	{
		delete[] m_pWorkerHandle;
		m_pWorkerHandle = NULL;
	}
}

bool IOCompletionPort::Initialize()
{
	WSADATA wsaData;
	int nResult;

	// winsock 2.2�������� �ʱ�ȭ
	nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (nResult != 0)
	{
		printf("[����] winsock �ʱ�ȭ ���� \n");
		return false;
	}

	//������ ����������
	m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_listenSocket == INVALID_SOCKET)
	{
		printf_s("[����] ���ϻ��� ����");
		return false;
	}

	//���� ���� ����
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	//���� ����
	nResult = bind(m_listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
	if (nResult == SOCKET_ERROR)
	{
		printf_s("[����] bind ���� \n");
		closesocket(m_listenSocket); // ���� �������Ŀ� �����ϸ� �ݾ��ְ� Ŭ���� �ؾ���.
		WSACleanup();
		return false;
	}

	// ���� ��⿭ ����
	nResult = listen(m_listenSocket, 5); //5 �� backlog
	if (nResult == SOCKET_ERROR)
	{
		printf_s("[����] listen ���� \n");
		closesocket(m_listenSocket); 
		WSACleanup();
		return false;
	}
	return true;
}

void IOCompletionPort::StartServer()
{
	int nResult;
	
	//Ŭ���̾�Ʈ ����
	SOCKADDR_IN clientAddr;
	int addrlen = sizeof(SOCKADDR_IN);
	SOCKET clientSocket;
	DWORD recvBytes;
	DWORD flags;

	//Completion Port ��ü ����, 
	//���ο� ����� �Ϸ� ��Ʈ�� �����Ҷ��� 
    //ù��°���� : ��ȿ�� �ڵ��� INVALID_HANDLE_VALUE ���� ����ص� �ȴ�.
	//�ι�°���� : NULL �̸� ���ο� ����� �Ϸ� ��Ʈ ����.
	//����°���� : ����� �Ϸ� ��Ŷ�� �� �ΰ� ������ 32��Ʈ ���� �� �� �ִ�. �񵿱� ����� �۾��� �Ϸ�ɶ����� �����Ǿ�
	//             ����� �Ϸ� ��Ʈ�� ����Ǵ� ����.
	//�׹�°���� : ���ÿ� ���� �Ҽ��ִ� �۾��� ������ ����. 0���� ������ �ڵ����� CPU ������ ���� ���� ������.
	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// Worker Thread ����
	if (!CreateWorkerThread()) return;

	printf_s("[INFO] ���� ����..\n");

	//Ŭ���̾�Ʈ ������ ����
	while (m_bAccept)
	{
		//�������ֱ� Accept()�κ� ���߿� winapi���� �ٲٸ��.
		clientSocket = WSAAccept(m_listenSocket, (struct sockaddr*)&clientAddr, &addrlen,NULL,NULL);

		if (clientSocket == INVALID_SOCKET)
		{
			printf_s("[����] Accept ���� \n");
			return;
		}
		printf_s("[Ŭ���̾�Ʈ ����] ip �ּ� = %s ��Ʈ��ȣ = %d \n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

		m_pSoketInfo = new stSOKETINFO();
		m_pSoketInfo->socket = clientSocket;
		m_pSoketInfo->recvBytes = 0;
		m_pSoketInfo->sendBytes = 0;
		m_pSoketInfo->dataBuf.len = MAX_BUFFER;
		m_pSoketInfo->dataBuf.buf = m_pSoketInfo->messageBuffer;
		flags = 0;

		m_hIOCP = CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCP, (DWORD)m_pSoketInfo, 0);

		//��ø ������ �����ϰ� �Ϸ�� ����� �Լ��� �Ѱ���.
		nResult = WSARecv(m_pSoketInfo->socket,		 // ����Ÿ �Է� ����
			             &m_pSoketInfo->dataBuf,	 // ����Ÿ �Է� ���� ������
			             1,							 // ����Ÿ �Է� ������ ��
			             &recvBytes,
			             &flags,
			             &(m_pSoketInfo->overlapped) // OVERLAPPED ���� ������
			             ,NULL);

		if (nResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			printf_s("[����] IO Pending ���� : %d \n", WSAGetLastError());
			return;
		}

	}
}

bool IOCompletionPort::CreateWorkerThread()
{
	unsigned int threadId;

	SYSTEM_INFO sysInfo; // �ý��� ������ ������
	GetSystemInfo(&sysInfo);
	printf_s("[INFO]CPU ���� : %d \n",sysInfo.dwNumberOfProcessors); 

	//�۾� ������ ���� (cpu ����*2 ��)+1
	int nThreadCnt = (sysInfo.dwNumberOfProcessors * 2) + 1;

	//thread handler ����
	m_pWorkerHandle = new HANDLE[nThreadCnt];
	
	//thread ����
	for (int i = 0; i < nThreadCnt; i++)
	{
		m_pWorkerHandle[i] = (HANDLE*)_beginthreadex(
			NULL, 0, &CallWorkerThread, this, CREATE_SUSPENDED, &threadId);

		//CREATE_SUSPENDED �÷��׸� ����Ͽ� ���μ��� ������ �ϰ� �Ǹ�,
        //���μ��� ������Ʈ�� ���������� ���� ������ ���� ����, �� ���� ���¿� �ְ� �ȴ�.

		if (m_pWorkerHandle[i] == NULL)
		{
			printf_s("[����] worker Thread ���� ���� \n");
			return false;
		}
		ResumeThread(m_pWorkerHandle[i]);
	}
	printf_s("[INFO] Worker Thread ����..\n");

	return true;
}

void IOCompletionPort::WorkerThread()
{
	//�Լ� ȣ�� ���� ���� �Ǵ�
	BOOL bResult;
	int nResult;

	//Overlaaped I/O �۾����� ���۵� ������ ũ��
	DWORD recvBytes;
	DWORD sendBytes;

	//Completion Key�� ���� ������ ���� 
	stSOKETINFO* pCompletionKey;

	// I/O �۾��� ���� ��û�� Overlapped ����ü�� ���� ������
	stSOKETINFO* pSocketInfo;

	//
	DWORD dwFlags = 0;

	while (m_bWorkerThread)
	{
		// �� �Լ��� ���� ��������� WaitingThread Queue �� �����·� ���� ��.
		// �Ϸ�� Overlapped I/O �۾��� �߻��ϸ� IOCP Queue ���� �Ϸ�� �۾��� �����ͼ� ��ó���� ��.
		bResult = GetQueuedCompletionStatus(m_hIOCP,
			&recvBytes,							//������ ���۵� ����Ʈ
			(LPDWORD)&pCompletionKey,			//completion Key
			(LPOVERLAPPED*)&pSocketInfo,		//overlapped I/O ��ü
			INFINITE							//��� �� �ð�
			);

		if (!bResult && recvBytes == 0)
		{
			printf_s("[INFO] socket(%d) ���� ���� \n", pSocketInfo->socket);
			closesocket(pSocketInfo->socket);
			free(pSocketInfo);
			continue;
		}
		
		pSocketInfo->dataBuf.len = recvBytes;

		if (recvBytes == 0)
		{
			closesocket(pSocketInfo->socket);
			free(pSocketInfo);
			continue;
		}
		else
		{
			printf_s("[INFO] �޽��� ���� - bytes : [%d], Msg : [%s] \n",
				pSocketInfo->dataBuf.len, pSocketInfo->dataBuf.buf);

			//Ŭ���̾�Ʈ�� ������ �״�� �۽�
			nResult = WSASend(
				pSocketInfo->socket,
				&(pSocketInfo->dataBuf),
				1,
				&sendBytes,
				dwFlags,
				NULL,
				NULL
			);

			if (nResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
			{
				printf_s("[����] WSASend ���� : %d \n", WSAGetLastError());
			}

			printf_s("[INFO] �޽��� �۽� - bytes : [%d], Msg : [%s] \n",
				 pSocketInfo->dataBuf.len, pSocketInfo->dataBuf.buf);

			//stSOCKETINFO ������ �ʱ�ȭ
			ZeroMemory(&(pSocketInfo->overlapped), sizeof(OVERLAPPED));
			pSocketInfo->dataBuf.len = MAX_BUFFER;
			pSocketInfo->dataBuf.buf = pSocketInfo->messageBuffer;

			ZeroMemory(pSocketInfo->messageBuffer, MAX_BUFFER);
			pSocketInfo->recvBytes = 0;
			pSocketInfo->sendBytes = 0;

			dwFlags = 0;

			// Ŭ���̾�Ʈ�κ��� �ٽ� ���� �ޱ����� WSARecv�� ȣ������.
			nResult = WSARecv(
				pSocketInfo->socket,
				&(pSocketInfo->dataBuf),
				1,
				&recvBytes,
				&dwFlags,
				(LPWSAOVERLAPPED)&(pSocketInfo->overlapped),
				NULL
			);

			if (nResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
			{
				printf_s("[����] WSARecv ���� : %d \n ", WSAGetLastError());
			}
		}
	}

}
