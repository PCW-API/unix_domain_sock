/**
 * @file sender.c
 * @brief UDS 클라이언트 송신 스레드
 *
 * 이 파일은 서버의 각 클라이언트에 대해 송신 큐를 확인하고,
 * 큐에 존재하는 데이터를 클라이언트 소켓을 통해 전송하는
 * 송신 처리 루틴을 정의합니다.
 */

#include "uds-server.h"
#include "uds.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <queue.h>

void* sendThread(void* arg) 
{
    UDS_SERVER* pstUdsServer = (UDS_SERVER *)arg;
    int iSendSize;
    while (1) {
        pthread_mutex_lock(&pstUdsServer->mutex);
        for (int i = 0; i < pstUdsServer->iMaxClients; ++i) {            
            if (pstUdsServer->pstClients[i].iActive && !queueIsEmpty(&(pstUdsServer->pstClients[i].stSendQueue))) {
                if(!queueIsEmpty(&(pstUdsServer->pstClients[i].stSendQueue))){
                    void* pvData = 0;
                    iSendSize = queuePop(&(pstUdsServer->pstClients[i].stSendQueue), &pvData);
                    udsSendMsg(pstUdsServer->pstClients[i].iSock, (char*)pvData, iSendSize);
                    free(pvData);
                }
            }
        }
        pthread_mutex_unlock(&pstUdsServer->mutex);
        usleep(5*1000); // 5ms
    }
    return NULL;
}
