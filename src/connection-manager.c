/**
 * @file connection-manager.c
 * @brief UDS 클라이언트 연결 관리 스레드
 *
 * 이 파일은 서버의 UDS 소켓을 통해 클라이언트 연결을 수락하고,
 * 서버가 관리하는 클라이언트 슬롯에 할당하는 스레드 함수 정의를 포함합니다.
 */

#include "uds-server.h"
#include "uds.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void* connectionManagerThread(void* arg) 
{
    UDS_SERVER* pstUdsServer = (UDS_SERVER *)arg;
    int iMaxClients = pstUdsServer->iMaxClients;
    while (1) {
        int iClientFd = acceptUdsClient(pstUdsServer->iServerSock);
        if (iClientFd >= 0) {
            pthread_mutex_lock(&pstUdsServer->mutex);
            for (int i = 0; i < iMaxClients; ++i) {                
                if (!pstUdsServer->pstClients[i].iActive) {
                    pstUdsServer->pstClients[i].iSock = iClientFd;                    
                    queueInit(&(pstUdsServer->pstClients[i].stRecvQueue), 10);
                    queueInit(&(pstUdsServer->pstClients[i].stSendQueue), 10);
                    pstUdsServer->pstClients[i].iActive = 1;
                    pstUdsServer->iClientCount++;
                    printf("[Connect] Client %d connected (fd: %d), count : %d\n", i, iClientFd, pstUdsServer->iClientCount);
                    break;
                }                
            }
            pthread_mutex_unlock(&pstUdsServer->mutex);
        } else {
            usleep(5*1000); // 5ms
        }
    }
    return NULL;
}