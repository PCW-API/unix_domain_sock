#ifndef UDS_SERVER_H
#define UDS_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include "queue.h"
#include "uds.h"

#define UDS_MAX_DATA_SIZE   1024    ///< 전송 가능한 최대 데이터 크기
#define QUEUE_SIZE          64      ///< 큐 버퍼 크기

/**
 * @brief 클라이언트 정보 구조체
 *
 * UDS 서버에 연결된 클라이언트의 소켓 상태와 송수신 큐를 저장합니다.
 */
typedef struct {
    int iSock;               ///< 클라이언트 소켓 디스크립터
    int iId;                 ///< 클라이언트 ID (인덱스 또는 식별자)
    int iActive;             ///< 클라이언트 활성화 여부
    QUEUE stSendQueue;       ///< 송신 큐
    QUEUE stRecvQueue;       ///< 수신 큐
} CLIENT;

/**
 * @brief UDS 서버 정보 구조체
 *
 * 서버 소켓과 연결된 클라이언트 목록 및 동기화를 위한 뮤텍스를 포함합니다.
 */
typedef struct {
    int iServerSock;         ///< 서버 소켓 디스크립터
    int iRunning;            ///< 서버 실행 상태 플래그
    int iMaxClients;         ///< 최대 클라이언트 수
    int iClientCount;        ///< 현재 연결된 클라이언트 수
    CLIENT *pstClients;      ///< 클라이언트 목록 포인터
    pthread_mutex_t mutex;   ///< 전체 서버 상태 보호용 뮤텍스
} UDS_SERVER;

/**
 * @brief 클라이언트 연결 관리 스레드 함수
 *
 * 서버 소켓에서 새로운 클라이언트 연결 요청을 수락하고,
 * 빈 슬롯이 있는 경우 클라이언트 배열에 등록합니다.
 * 연결된 소켓은 CLIENT 구조체에 저장되며, 
 * 해당 클라이언트의 활성 플래그를 설정합니다.
 *
 * @param arg UDS_SERVER 구조체 포인터
 * @return NULL
 */
void* connectionManagerThread(void* arg);

/**
 * @brief 수신 스레드 함수
 *
 * poll()을 통해 다중 클라이언트의 소켓에서 수신 가능 여부를 감지하고,
 * 수신된 데이터를 클라이언트의 수신 큐에 저장합니다.
 * 클라이언트가 비정상적으로 종료된 경우 활성 상태를 false로 설정합니다.
 *
 * @param arg UDS_SERVER 구조체 포인터
 * @return NULL
 */
void* recvThread(void* arg);

/**
 * @brief 송신 스레드 함수
 *
 * 주기적으로 모든 클라이언트의 송신 큐를 확인하고,
 * 큐에 데이터가 존재하면 해당 데이터를 클라이언트 소켓을 통해 전송합니다.
 * 전송 실패 시 클라이언트를 비활성화 처리합니다.
 *
 * @param arg UDS_SERVER 구조체 포인터
 * @return NULL
 */
void* sendThread(void* arg);


void startUdsServer(UDS_SERVER *pstUdsServer, char* pchUdsPath, int iUdsClientCount);
void stopUdsServer(UDS_SERVER *pstUdsServer);

#ifdef __cplusplus
}
#endif

#endif