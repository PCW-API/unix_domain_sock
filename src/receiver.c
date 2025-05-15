/**
 * @file receiver.c
 * @brief UDS 클라이언트 수신 스레드
 *
 * 이 파일은 poll() 시스템 콜을 사용하여 다수의 클라이언트로부터
 * 데이터를 비동기적으로 수신하고, 수신된 데이터를 수신 큐에 저장하는
 * 스레드 함수를 정의합니다.
 */

#include "uds-server.h"
#include "uds.h"
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

void* recvThread(void* arg) 
{
    UDS_SERVER* pstUdsServer = (UDS_SERVER *)arg;
    int iMaxClients = pstUdsServer->iMaxClients;    
    struct pollfd stPollFds[iMaxClients];

    while (1) {
        int iPollCount = 0;
        pthread_mutex_lock(&pstUdsServer->mutex);
        for (int iPollFdIndex = 0; iPollFdIndex < iMaxClients; iPollFdIndex++) {
            if (pstUdsServer->pstClients[iPollFdIndex].iActive) {                
                stPollFds[iPollCount].fd = pstUdsServer->pstClients[iPollFdIndex].iSock;
                stPollFds[iPollCount].events = POLLIN;
                iPollCount++;
            }            
        }
        pthread_mutex_unlock(&pstUdsServer->mutex);

        int iReady = poll(stPollFds, iPollCount, 500);
        if (iReady <= 0)
            continue;

        for (int iPollFdIndex = 0; iPollFdIndex < iPollCount; iPollFdIndex++) {
            if (stPollFds[iPollFdIndex].revents & POLLIN) {
                int iClientFd = stPollFds[iPollFdIndex].fd;
                char chBuffer[1024];
                int iRecvSize = udsRecvMsg(iClientFd, chBuffer, sizeof(chBuffer) - 1);
                if (iRecvSize <= 0) {                    
                    close(iClientFd);
                    pthread_mutex_lock(&pstUdsServer->mutex);
                    for (int iClientFdIndex = 0; iClientFdIndex < pstUdsServer->iMaxClients; iClientFdIndex++) {
                        if (pstUdsServer->pstClients[iClientFdIndex].iSock == iClientFd) {
                            pstUdsServer->pstClients[iClientFdIndex].iActive = 0;
                            queueDestroy(&(pstUdsServer->pstClients[iClientFdIndex].stSendQueue));
                            queueDestroy(&(pstUdsServer->pstClients[iClientFdIndex].stRecvQueue));
                            pstUdsServer->pstClients[iClientFdIndex].iSock = -1;
                            pstUdsServer->iClientCount--;
                            break;
                        }                        
                    }
                    pthread_mutex_unlock(&pstUdsServer->mutex);
                } else {                    
                    chBuffer[iRecvSize] = '\0';
                    void* pvData = malloc(iRecvSize);
                    if (pvData == NULL) {
                        fprintf(stderr, "Memory allocation failed\n");
                    } else {
                        memcpy(pvData, chBuffer, iRecvSize);
                        pthread_mutex_lock(&pstUdsServer->mutex);                    
                        for (int iClientFdIndex = 0; iClientFdIndex < pstUdsServer->iMaxClients; iClientFdIndex++) {
                            if (pstUdsServer->pstClients[iClientFdIndex].iSock == iClientFd) {
                                if(queuePush(&(pstUdsServer->pstClients[iClientFdIndex].stRecvQueue), pvData, iRecvSize) == 0){
                                    fprintf(stderr,"### FAIL %s():%d Msg:%s ###\n", __func__,__LINE__, chBuffer);
                                    break;
                                }
                            }
                        }
                        pthread_mutex_unlock(&pstUdsServer->mutex);
                    }                    
                }
            }
        }
    }
    return NULL;
}
