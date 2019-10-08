#pragma once
#include <WinSock2.h>

#define	MAX_BUFFER		1024
#define SERVER_PORT		9000

struct stSOKETINFO
{
	WSAOVERLAPPED	overlapped;
	WSABUF			dataBuf;
	SOCKET			socket;
	char			messageBuffer[MAX_BUFFER];
	int				recvBytes;
	int				sendBytes;
};

class USER_INFO
{
public:
	int index;
	int len;
};

class IOCompletionPort
{
public:
	IOCompletionPort();
	~IOCompletionPort();

public:
	// ���� ��� �� ���� ���� ����
	bool Initialize();
	
	// ���� ����
	void StartServer();
	
	// �۾������� ����
	bool CreateWorkerThread();

	// �۾� ������
	void WorkerThread();

private:
	stSOKETINFO*		m_pSoketInfo;	 // ���� ����
	SOCKET				m_listenSocket;  // ���� ���� ����
	HANDLE				m_hIOCP;		 // IOCP ��ü �ڵ�
	bool				m_bAccept;		 // ��û ���� �÷���
	bool				m_bWorkerThread; // �۾� ������ ���� �÷���
	HANDLE*				m_pWorkerHandle; // �۾� ������ �ڵ�
private:
	//���߿� ��üȭ �и� ����
	int				    	m_iIndex; // ���� �ε�����
	

};

