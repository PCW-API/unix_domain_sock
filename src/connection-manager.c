// connection_manager.c
#include "uds-server.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void* connectionManagerThread(void* arg) {
    UDS_SERVER* pstUdsServer = (UDS_SERVER *)arg;
    while (1) {
        int iClientFd = acceptUdsClient(pstUdsServer->iServerSock);
        if (iClientFd >= 0) {
            for (int i = 0; i < pstUdsServer->iMaxClients; ++i) {
                if (!pstUdsServer->pstClients[i].iActive) {
                    pstUdsServer->pstClients[i].iSock = iClientFd;
                    queueInit(&(pstUdsServer->pstClients[i].stRecvQueue), 10);
                    queueInit(&(pstUdsServer->pstClients[i].stSendQueue), 10);
                    pstUdsServer->pstClients[i].iActive = 1;
                    printf("[Connect] Client %d connected (fd: %d)\n", i, iClientFd);
                    break;
                }
            }
        } else {
            usleep(5*1000); // 5ms
        }
    }
    return NULL;
}