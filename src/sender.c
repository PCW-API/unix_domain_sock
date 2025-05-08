// send_handler.c
#include "uds-server.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void* send_thread(void* arg) 
{
    UDS_SERVER* pstUdsServer = (UDS_SERVER *)arg;
    int iSendSize;
    while (1) {
        for (int i = 0; i < pstUdsServer->iMaxClients; ++i) {
            if (pstUdsServer->pstClients[i].iActive && !queueIsEmpty(&(pstUdsServer->pstClients[i].stSendQueue))) {
                if(!queueIsEmpty(&(pstUdsServer->pstClients[i].stSendQueue))){
                    void* pvData = 0;
                    iSendSize = queuePop(&(pstUdsServer->pstClients[i].stSendQueue), &pvData);
                    udsSendMsg(pstUdsServer->pstClients[i].iSock, pvData, iSendSize);
                    free(pvData);
                }
            }
        }
        usleep(5*1000); // 5ms
    }
    return NULL;
}
