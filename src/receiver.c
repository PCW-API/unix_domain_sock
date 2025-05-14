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
        for (int i = 0; i < iMaxClients; ++i) {            
            if (pstUdsServer->pstClients[i].iActive) {
                stPollFds[iPollCount].fd = pstUdsServer->pstClients[i].iSock;
                stPollFds[iPollCount].events = POLLIN;
                iPollCount++;
            }            
        }
        pthread_mutex_unlock(&pstUdsServer->mutex);

        int iReady = poll(stPollFds, iPollCount, 500);
        if (iReady <= 0)
            continue;

        for (int i = 0; i < iPollCount; ++i) {
            if (stPollFds[i].revents & POLLIN) {
                int iClientFd = stPollFds[i].fd;
                char chBuffer[1024];
                int iRecvSize = udsRecvMsg(iClientFd, chBuffer, sizeof(chBuffer) - 1);
                if (iRecvSize <= 0) {
                    close(iClientFd);
                    pthread_mutex_lock(&pstUdsServer->mutex);
                    for (int j = 0; j < pstUdsServer->iMaxClients; ++j) {
                        if (pstUdsServer->pstClients[j].iSock == iClientFd) {
                            pstUdsServer->pstClients[j].iActive = 0;
                            queueDestroy(&(pstUdsServer->pstClients[i].stSendQueue));
                            queueDestroy(&(pstUdsServer->pstClients[i].stRecvQueue));
                            pstUdsServer->pstClients[j].iSock = -1;
                            pstUdsServer->iClientCount--;                            
                        }
                        break;
                    }
                    pthread_mutex_unlock(&pstUdsServer->mutex);
                } else {                    
                    chBuffer[iRecvSize] = '\0';                    
                    pthread_mutex_lock(&pstUdsServer->mutex);
                    // fprintf(stderr,"### %s():%d Msg:%s [%d %d]###\n", __func__,__LINE__, chBuffer, pstUdsServer->pstClients[i].iSock, iClientFd);
                    for (int j = 0; j < pstUdsServer->iMaxClients; ++j) {
                        if (pstUdsServer->pstClients[i].iSock == iClientFd) {
                            if(queuePush(&(pstUdsServer->pstClients[i].stRecvQueue), strdup(chBuffer), iRecvSize) == 0){
                                fprintf(stderr,"### FAIL %s():%d Msg:%s ###\n", __func__,__LINE__, chBuffer);
                            }
                            break;
                        }
                    }
                    pthread_mutex_unlock(&pstUdsServer->mutex);
                }
            }
        }
    }
    return NULL;
}
