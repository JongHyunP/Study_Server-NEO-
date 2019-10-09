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
	// winsock의 사용을 끝낸다.
	WSACleanup();

	//다 사용한 객체를 삭제
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

	// winsock 2.2버젼으로 초기화
	nResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (nResult != 0)
	{
		printf("[에러] winsock 초기화 실패 \n");
		return false;
	}

	//소켓을 생성해주자
	m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_listenSocket == INVALID_SOCKET)
	{
		printf_s("[에러] 소켓생성 실패");
		return false;
	}

	//서버 정보 설정
	SOCKADDR_IN serverAddr;
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	//소켓 설정
	nResult = bind(m_listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN));
	if (nResult == SOCKET_ERROR)
	{
		printf_s("[에러] bind 실패 \n");
		closesocket(m_listenSocket); // 소켓 생성이후에 실패하면 닫아주고 클린업 해야함.
		WSACleanup();
		return false;
	}

	// 수신 대기열 생성
	nResult = listen(m_listenSocket, 5); //5 는 backlog
	if (nResult == SOCKET_ERROR)
	{
		printf_s("[에러] listen 실패 \n");
		closesocket(m_listenSocket); 
		WSACleanup();
		return false;
	}
	return true;
}

void IOCompletionPort::StartServer()
{
	int nResult;
	
	//클라이언트 정보
	SOCKADDR_IN clientAddr;
	int addrlen = sizeof(SOCKADDR_IN);
	SOCKET clientSocket;
	DWORD recvBytes;
	DWORD flags;

	//Completion Port 객체 생성, 
	//새로운 입출력 완료 포트를 생성할때는 
    //첫번째인자 : 유효한 핸들대신 INVALID_HANDLE_VALUE 값을 사용해도 된다.
	//두번째인자 : NULL 이면 새로운 입출력 완료 포트 생성.
	//세번째인자 : 입출력 완료 패킷에 들어갈 부가 정보로 32비트 값을 줄 수 있다. 비동기 입출력 작업이 완료될때마다 생성되어
	//             입출력 완료 포트에 저장되는 정보.
	//네번째인자 : 동시에 실행 할수있는 작업자 스레드 개수. 0으로 넣으면 자동으로 CPU 개수와 같은 수로 설정됨.
	m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// Worker Thread 생성
	if (!CreateWorkerThread()) return;

	printf_s("[INFO] 서버 시작..\n");

	//클라이언트 접속을 받음
	while (m_bAccept)
	{
		//연결해주기 Accept()부분 나중에 winapi에서 바꾸면됨.
		clientSocket = WSAAccept(m_listenSocket, (struct sockaddr*)&clientAddr, &addrlen,NULL,NULL);

		if (clientSocket == INVALID_SOCKET)
		{
			printf_s("[에러] Accept 실패 \n");
			return;
		}
		printf_s("[클라이언트 접속] ip 주소 = %s 포트번호 = %d \n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

		m_pSoketInfo = new stSOKETINFO();
		m_pSoketInfo->socket = clientSocket;
		m_pSoketInfo->recvBytes = 0;
		m_pSoketInfo->sendBytes = 0;
		m_pSoketInfo->dataBuf.len = MAX_BUFFER;
		m_pSoketInfo->dataBuf.buf = m_pSoketInfo->messageBuffer;
		flags = 0;

		m_hIOCP = CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCP, (DWORD)m_pSoketInfo, 0);

		//중첩 소켓을 지정하고 완료시 실행될 함수를 넘겨줌.
		nResult = WSARecv(m_pSoketInfo->socket,		 // 데이타 입력 소켓
			             &m_pSoketInfo->dataBuf,	 // 데이타 입력 버퍼 포인터
			             1,							 // 데이타 입력 버퍼의 수
			             &recvBytes,
			             &flags,
			             &(m_pSoketInfo->overlapped) // OVERLAPPED 변수 포인터
			             ,NULL);

		if (nResult == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			printf_s("[에러] IO Pending 실패 : %d \n", WSAGetLastError());
			return;
		}

	}
}

bool IOCompletionPort::CreateWorkerThread()
{
	unsigned int threadId;

	SYSTEM_INFO sysInfo; // 시스템 정보를 가져옴
	GetSystemInfo(&sysInfo);
	printf_s("[INFO]CPU 갯수 : %d \n",sysInfo.dwNumberOfProcessors); 

	//작업 스레드 갯수 (cpu 개수*2 개)+1
	int nThreadCnt = (sysInfo.dwNumberOfProcessors * 2) + 1;

	//thread handler 선언
	m_pWorkerHandle = new HANDLE[nThreadCnt];
	
	//thread 생성
	for (int i = 0; i < nThreadCnt; i++)
	{
		m_pWorkerHandle[i] = (HANDLE*)_beginthreadex(
			NULL, 0, &CallWorkerThread, this, CREATE_SUSPENDED, &threadId);

		//CREATE_SUSPENDED 플래그를 명시하여 프로세스 생성을 하게 되면,
        //프로세스 오브젝트는 생성되지만 아직 실행은 되지 않은, 즉 중지 상태에 있게 된다.

		if (m_pWorkerHandle[i] == NULL)
		{
			printf_s("[에러] worker Thread 생성 실패 \n");
			return false;
		}
		ResumeThread(m_pWorkerHandle[i]);
	}
	printf_s("[INFO] Worker Thread 시작..\n");

	return true;
}

void IOCompletionPort::WorkerThread()
{
	//함수 호출 성공 여부 판단
	BOOL bResult;
	int nResult;

	//Overlaaped I/O 작업에서 전송된 데이터 크기
	DWORD recvBytes;
	DWORD sendBytes;

	//Completion Key를 받을 포인터 변수 
	stSOKETINFO* pCompletionKey;

	// I/O 작업을 위해 요청한 Overlapped 구조체를 받을 포인터
	stSOKETINFO* pSocketInfo;

	//
	DWORD dwFlags = 0;

	while (m_bWorkerThread)
	{
		// 이 함수로 인해 스레드들은 WaitingThread Queue 에 대기상태로 들어가게 됨.
		// 완료된 Overlapped I/O 작업이 발생하면 IOCP Queue 에서 완료된 작업을 가져와서 뒤처리를 함.
		bResult = GetQueuedCompletionStatus(m_hIOCP,
			&recvBytes,							//실제로 전송된 바이트
			(LPDWORD)&pCompletionKey,			//completion Key
			(LPOVERLAPPED*)&pSocketInfo,		//overlapped I/O 객체
			INFINITE							//대기 할 시간
			);

		if (!bResult && recvBytes == 0)
		{
			printf_s("[INFO] socket(%d) 접속 끊김 \n", pSocketInfo->socket);
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
			printf_s("[INFO] 메시지 수신 - bytes : [%d], Msg : [%s] \n",
				pSocketInfo->dataBuf.len, pSocketInfo->dataBuf.buf);

			//클라이언트의 응답을 그대로 송신
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
				printf_s("[에러] WSASend 실패 : %d \n", WSAGetLastError());
			}

			printf_s("[INFO] 메시지 송신 - bytes : [%d], Msg : [%s] \n",
				 pSocketInfo->dataBuf.len, pSocketInfo->dataBuf.buf);

			//stSOCKETINFO 데이터 초기화
			ZeroMemory(&(pSocketInfo->overlapped), sizeof(OVERLAPPED));
			pSocketInfo->dataBuf.len = MAX_BUFFER;
			pSocketInfo->dataBuf.buf = pSocketInfo->messageBuffer;

			ZeroMemory(pSocketInfo->messageBuffer, MAX_BUFFER);
			pSocketInfo->recvBytes = 0;
			pSocketInfo->sendBytes = 0;

			dwFlags = 0;

			// 클라이언트로부터 다시 응답 받기위해 WSARecv를 호출해줌.
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
				printf_s("[에러] WSARecv 실패 : %d \n ", WSAGetLastError());
			}
		}
	}

}
