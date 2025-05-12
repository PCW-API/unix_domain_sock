#ifndef UDS_SERVER_H
#define UDS_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include "queue.h"

#define UDS_MAX_DATA_SIZE   1024
#define QUEUE_SIZE          64


typedef struct {
    int iSock;
    int iId;
    int iActive;
    QUEUE stSendQueue;
    QUEUE stRecvQueue;
} CLIENT;

typedef struct {
    int iServerSock;
    int iRunning;
    int iMaxClients;
    CLIENT *pstClients;
    pthread_t threadId;
    pthread_mutex_t clientLock;
} UDS_SERVER;

void* recvThread(void* arg);
void* send_thread(void* arg);

#ifdef __cplusplus
}
#endif

#endif