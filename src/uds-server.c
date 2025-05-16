#include "uds-server.h"
#include <stdlib.h>
#include <unistd.h>

void startUdsServer(UDS_SERVER *pstUdsServer, char* pchUdsPath, int iUdsClientCount) 
{
    unlink(pchUdsPath);
    pstUdsServer->iServerSock = createUdsServerSocket(pchUdsPath, iUdsClientCount);
    pthread_mutex_init(&pstUdsServer->mutex, NULL);
    pstUdsServer->iMaxClients = iUdsClientCount;
    pstUdsServer->iRunning = 0;
    pstUdsServer->iClientCount = 0;
    pstUdsServer->pstClients = (CLIENT *)malloc(sizeof(CLIENT) * iUdsClientCount);
    for (int i = 0; i < pstUdsServer->iMaxClients; ++i) {
        pstUdsServer->pstClients[i].iSock = -1;
        pstUdsServer->pstClients[i].iActive = 0;
        pstUdsServer->pstClients[i].iId = -1;
    }
}

void stopUdsServer(UDS_SERVER *pstUdsServer) 
{
    pstUdsServer->iRunning = 0;
    pstUdsServer->iClientCount = 0;
    
    if (pstUdsServer->iServerSock) {
        udsClose(pstUdsServer->iServerSock);
    }
    free(pstUdsServer->pstClients);
}