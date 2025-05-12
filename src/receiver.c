// recv_handler.c
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
    struct pollfd stPollFds[pstUdsServer->iMaxClients];

    while (1) {
        int iPollCount = 0;
        for (int i = 0; i < pstUdsServer->iMaxClients; ++i) {
            if (pstUdsServer->pstClients[i].iActive) {
                stPollFds[iPollCount].fd = pstUdsServer->pstClients[i].iSock;
                stPollFds[iPollCount].events = POLLIN;
                iPollCount++;
            }
        }

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
                    for (int j = 0; j < pstUdsServer->iMaxClients; ++j) {
                        if (pstUdsServer->pstClients[j].iSock == iClientFd) {
                            pstUdsServer->pstClients[j].iActive = 0;
                            queueDestroy(&(pstUdsServer->pstClients[i].stSendQueue));
                            queueDestroy(&(pstUdsServer->pstClients[i].stRecvQueue));
                            pstUdsServer->pstClients[j].iSock = -1;
                        }
                    }
                } else {
                    chBuffer[iRecvSize] = '\0';
                    for (int j = 0; j < pstUdsServer->iMaxClients; ++j) {
                        if (pstUdsServer->pstClients[i].iSock == iClientFd) {
                            queuePush(&(pstUdsServer->pstClients[i].stRecvQueue), strdup(chBuffer), iRecvSize);                            
                        }
                    }
                }
            }
        }
    }
    return NULL;
}
