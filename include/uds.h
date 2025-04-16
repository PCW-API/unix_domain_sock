#ifndef UDS_H
#define UDS_H

#include <stddef.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unix 도메인 소켓 서버를 생성 및 클라이언트 연결 대기
 *
 * @param pchSocketPath Unix 도메인 소켓 파일 경로.
 * @return 연결된 클라이언트 소켓 디스크립터.
 */
int createUdsServerSocket(const char *pchSocketPath, int iMaxClients);

/**
 * @brief Unix 도메인 소켓 서버에 연결
 *
 * @param pchSocketPath Unix 도메인 소켓 파일 경로.
 * @return 연결된 소켓 디스크립터.
 */
int createUdsClientSocket(const char *pchSocketPath);

/**
 * @brief Unix 도메인 소켓을 클라이언트 접속 종료 
 *
 * @param iClientSockFd 클라이언트 소켓 디스크립터.
 */
void handleUdsClientDisconnection(int iClientSockFd);

/**
 * @brief Unix 도메인 소켓을 통해 메시지를 전송
 *
 * @param iSock 소켓 디스크립터.
 * @param pchData 전송할 메시지.
 * @param iLength 메시지의 길이.
 * @return 전송된 바이트 수.
 */
int udsSendMsg(int iSock, const char *pchData, size_t iLength);

/**
 * @brief Unix 도메인 소켓을 통해 메시지를 수신
 *
 * @param iSock 소켓 디스크립터.
 * @param pchData 수신된 메시지를 저장할 버퍼.
 * @param iLength 버퍼의 길이.
 * @return 수신된 바이트 수.
 */
int udsRecvMsg(int iSock, char *pchData, size_t iLength);

/**
 * @brief Unix 도메인 소켓을 통해 메시지 수신(지정한 시간만큼 대기 후 타임아웃 처리).
 *
 * @param iSock 데이터를 수신할 소켓 디스크립터
 * @param pchData 수신 데이터를 저장할 버퍼 포인터
 * @param iLength 수신할 바이트 수
 * @param iTimeoutMsec 타임아웃 시간(밀리초)
 * @return 성공 시 수신한 바이트 수, 타임아웃 시 0, 실패 시 -1 반환
 */
int udsRecvMsgTimeout(int iSock, char *pchData, size_t iLength, int iTimeoutMsec);

/**
 * @brief Unix 도메인 소켓을 종료합니다.
 *
 * @param iSock 종료할 소켓 디스크립터.
 */
void udsClose(int iSock);


int acceptUdsClient(int iServerFd) ;

#ifdef __cplusplus
}
#endif

#endif
