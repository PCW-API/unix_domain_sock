#include "uds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

// #include <sys/types.h>
#include <sys/select.h>
// #include <fcntl.h>

int createUdsServerSocket(const char *pchSocketPath, int iMaxClients) {
    int iServerFd;
    struct sockaddr_un address;
    
    if ((iServerFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, pchSocketPath, sizeof(address.sun_path) - 1);
    unlink(pchSocketPath);

    if (bind(iServerFd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("Bind failed");
        close(iServerFd);
        exit(EXIT_FAILURE);
    }

    if (listen(iServerFd, iMaxClients) == -1) {
        perror("Listen failed");
        close(iServerFd);
        exit(EXIT_FAILURE);
    }
    return iServerFd;
}

int acceptUdsClient(int iServerFd) 
{
    int iClientSockFd;
    struct sockaddr_un stClientSockAddr;
    socklen_t uiClientAddrLen = sizeof(struct sockaddr_un);

    if ((iClientSockFd = accept(iServerFd, (struct sockaddr *)&stClientSockAddr, &uiClientAddrLen)) == -1) {
        perror("Accept failed");
    }

    return iClientSockFd;
}

int createUdsClientSocket(const char *pchSocketPath) {
    int iSock;
    struct sockaddr_un address;
    
    if ((iSock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, pchSocketPath, sizeof(address.sun_path) - 1);

    if (connect(iSock, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("Connection failed");
        close(iSock);
        exit(EXIT_FAILURE);
    }
    return iSock;
}

void handleUdsClientDisconnection(int iClientSockFd) 
{
    fprintf(stderr,"%d client connection was terminated.\n", iClientSockFd);
    close(iClientSockFd);    
}

int udsSendMsg(int iSock, const char *pchData, size_t iLength) {
    return send(iSock, pchData, iLength, 0);
}

int udsRecvMsg(int iSock, char *pchData, size_t iLength) {
    return recv(iSock, pchData, iLength, 0);
}


int udsRecvMsgTimeout(int iSock, char *pchData, size_t iLength, int iTimeoutMsec) 
{
    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(iSock, &read_fds);

    timeout.tv_sec = 0;
    timeout.tv_usec = (iTimeoutMsec * 1000);

    int ret = select(iSock + 1, &read_fds, NULL, NULL, &timeout);
    if (ret < 0) {
        perror("select error");
        return -1;
    } else if (ret == 0) {
        fprintf(stderr, "Timeout: no data received within %d seconds.\n", iTimeoutMsec);
        return UDS_TIME_OUT;  // timeout
    }

    ssize_t received = recv(iSock, pchData, iLength, 0);
    if (received < 0) {
        perror("recv failed");
        return -1;
    }

    return (int)received;
}
void udsClose(int iSock) {
    close(iSock);
}
