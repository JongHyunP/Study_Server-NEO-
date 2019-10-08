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
	// 소켓 등록 및 서버 정보 설정
	bool Initialize();
	
	// 서버 시작
	void StartServer();
	
	// 작업스레드 생성
	bool CreateWorkerThread();

	// 작업 스레드
	void WorkerThread();

private:
	stSOKETINFO*		m_pSoketInfo;	 // 소켓 정보
	SOCKET				m_listenSocket;  // 서버 리슨 소켓
	HANDLE				m_hIOCP;		 // IOCP 객체 핸들
	bool				m_bAccept;		 // 요청 동작 플래그
	bool				m_bWorkerThread; // 작업 스레드 동작 플래그
	HANDLE*				m_pWorkerHandle; // 작업 스레드 핸들
private:
	//나중에 객체화 분리 예정
	int				    	m_iIndex; // 유저 인덱스용
	

};

