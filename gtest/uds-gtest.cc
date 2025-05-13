#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "../include/uds.h"
#include "../include/uds-server.h"
#include "queue.h"

constexpr const char* TEST_SOCKET_PATH = "/tmp/test_socket";
constexpr int TEST_CLIENT_COUNT = 5;
constexpr int CONNECT_WAIT_MS = 100;
constexpr int DATA_WAIT_MS = 300;

extern UDS_SERVER* g_pstUdsServer;

void startUdsServer() {    
    g_pstUdsServer->iServerSock = createUdsServerSocket(TEST_SOCKET_PATH, TEST_CLIENT_COUNT);
    g_pstUdsServer->iMaxClients = TEST_CLIENT_COUNT;
    g_pstUdsServer->iRunning = 0;
    g_pstUdsServer->pstClients = nullptr;
    ASSERT_NE(g_pstUdsServer, nullptr);
    std::thread(&sendThread, g_pstUdsServer).detach();
    std::thread(&recvThread, g_pstUdsServer).detach();
    std::thread(&connectionManagerThread, g_pstUdsServer).detach();
}

void stopUdsServer() {
    g_pstUdsServer->iRunning = 0;
    // g_pstUdsServer->pstClients
    if (g_pstUdsServer->iServerSock) {
        udsClose(g_pstUdsServer->iServerSock);
        g_pstUdsServer = nullptr;
    }
}

int createTestClientSocket() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, TEST_SOCKET_PATH);
    int retry = 0;
    while (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0 && retry++ < 10)
        std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_WAIT_MS));
    return sock;
}

// Common Fixture
class UdsServerTest : public ::testing::Test {
protected:
    std::vector<int> clientSockets;
    std::vector<std::thread> clientThreads;
    std::vector<std::string> testData;
    std::vector<std::string> receivedData;
    std::atomic<bool> running{true};

    void SetUp() override {
        startUdsServer();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    void TearDown() override {
        running = false;
        for (auto& t : clientThreads) if (t.joinable()) t.join();
        for (int sock : clientSockets) if (sock > 0) close(sock);
        stopUdsServer();
    }

    void createClients(int count) {
        for (int i = 0; i < count; ++i) {
            int sock = createTestClientSocket();
            ASSERT_GT(sock, 0) << "Client " << i << " connection failed.";
            clientSockets.push_back(sock);
        }
    }

    void createClientsAndSendData(int count) {
        for (int i = 0; i < count; ++i) {
            int sock = createTestClientSocket();
            ASSERT_GT(sock, 0);
            clientSockets.push_back(sock);
            std::string msg = "Client_" + std::to_string(i + 1) + "_DATA";
            testData.push_back(msg);
            send(sock, msg.c_str(), msg.size(), 0);
        }
    }

    void collectReceivedData() {
        std::this_thread::sleep_for(std::chrono::milliseconds(DATA_WAIT_MS));
        for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
            if (g_pstUdsServer->pstClients[i].iActive) {
                void* data = nullptr;
                int size = queuePop(&g_pstUdsServer->pstClients[i].stRecvQueue, &data);
                if (size > 0 && data) {
                    receivedData.emplace_back(std::string((char*)data, size));
                    free(data);
                }
            }
        }
    }

    void clientLoopbackThread(int sock) {
        char buf[128] = {0};
        while (running) {
            int len = recv(sock, buf, sizeof(buf), 0);
            if (len > 0) send(sock, buf, len, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void createClientsWithLoopback(int count) {
        for (int i = 0; i < count; ++i) {
            int sock = createTestClientSocket();
            ASSERT_GT(sock, 0);
            clientSockets.push_back(sock);
            clientThreads.emplace_back(&UdsServerTest::clientLoopbackThread, this, sock);
        }
    }
};

// Test 1: Client Connect and Disconnect
TEST_F(UdsServerTest, ClientConnectDisconnectTest) {
    createClients(TEST_CLIENT_COUNT);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(g_pstUdsServer->iClientCount, TEST_CLIENT_COUNT);
    for (size_t i = 0; i < clientSockets.size(); ++i) {
        close(clientSockets[i]);
        clientSockets[i] = -1;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int expected = static_cast<int>(clientSockets.size() - (i + 1));
        ASSERT_EQ(g_pstUdsServer->iClientCount, expected);
    }
    ASSERT_EQ(g_pstUdsServer->iClientCount, 0);
}

// Test 2: Client -> Server Receive
TEST_F(UdsServerTest, ClientSendDataTest) {
    createClientsAndSendData(TEST_CLIENT_COUNT);
    collectReceivedData();
    ASSERT_EQ(receivedData.size(), testData.size());
    for (const auto& expected : testData) {
        bool found = false;
        for (const auto& actual : receivedData)
            if (actual == expected) found = true;
        EXPECT_TRUE(found) << "Missing: " << expected;
    }
}

// Test 3: Server -> Client Loopback
TEST_F(UdsServerTest, ServerClientLoopback) {
    createClientsWithLoopback(TEST_CLIENT_COUNT);
    for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
        if (g_pstUdsServer->pstClients[i].bUsed) {
            std::string msg = "ServerData_" + std::to_string(i);
            testData.push_back(msg);
            char* data = strdup(msg.c_str());
            queuePush(&g_pstUdsServer->pstClients[i].stSendQueue, data, msg.size());
        }
    }
    collectReceivedData();
    int matched = 0;
    for (const auto& expected : testData) {
        for (const auto& actual : receivedData)
            if (expected == actual) matched++;
    }
    ASSERT_EQ(matched, TEST_CLIENT_COUNT);
}

// Test 4: Unexpected Client Exit
TEST_F(UdsServerTest, ClientUnexpectedTermination) {
    pid_t pid = fork();
    if (pid == 0) {
        int sock = createTestClientSocket();
        (void)sock;
        _exit(0); // child exits without close
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        ASSERT_LE(g_pstUdsServer->iClientCount, TEST_CLIENT_COUNT - 1);
    }
}


// Test 5: Fragmented Send Test (partial message send)
TEST_F(UdsServerTest, FragmentedSendTest) {
    int sock = createTestClientSocket();
    ASSERT_GT(sock, 0);
    clientSockets.push_back(sock);
    const char* part1 = "Part1_";
    const char* part2 = "Part2_END";
    send(sock, part1, strlen(part1), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    send(sock, part2, strlen(part2), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(DATA_WAIT_MS));
    void* data = nullptr;
    int size = queuePop(&g_pstUdsServer->pstClients[0].stRecvQueue, &data);
    ASSERT_GT(size, 0);
    std::string msg((char*)data, size);
    free(data);

    EXPECT_TRUE(msg.find("Part1_") != std::string::npos);
    EXPECT_TRUE(msg.find("Part2_END") != std::string::npos);
}

// Test 6: Simultaneous Multi-client Send
TEST_F(UdsServerTest, SimultaneousClientSendTest) {
    createClients(TEST_CLIENT_COUNT);
    std::vector<std::thread> senders;
    for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
        senders.emplace_back([sock=clientSockets[i], i]() {
            std::string msg = "Simultaneous_Client_" + std::to_string(i);
            send(sock, msg.c_str(), msg.size(), 0);
        });
    }
    for (auto& t : senders) t.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(DATA_WAIT_MS));
    int received = 0;
    for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
        void* data = nullptr;
        int size = queuePop(&g_pstUdsServer->pstClients[i].stRecvQueue, &data);
        if (size > 0 && data) {
            received++;
            free(data);
        }
    }
    ASSERT_EQ(received, TEST_CLIENT_COUNT);
}

// Test 7: Send Queue Saturation
TEST_F(UdsServerTest, SendQueueSaturationTest) {
    int sock = createTestClientSocket();
    ASSERT_GT(sock, 0);
    clientSockets.push_back(sock);

    for (int i = 0; i < 1000; ++i) {
        std::string msg = "FloodData_" + std::to_string(i);
        char* data = strdup(msg.c_str());
        queuePush(&g_pstUdsServer->pstClients[0].stSendQueue, data, msg.size());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // No crash or memory issue means test passed; optionally validate with metrics
    SUCCEED();
}

// Test 8: Client Reconnection
TEST_F(UdsServerTest, ClientReconnectTest) {
    int sock = createTestClientSocket();
    ASSERT_GT(sock, 0);
    close(sock);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int sock2 = createTestClientSocket();
    ASSERT_GT(sock2, 0);
    clientSockets.push_back(sock2);
    std::string msg = "ReconnectTest";
    send(sock2, msg.c_str(), msg.size(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    void* data = nullptr;
    int size = queuePop(&g_pstUdsServer->pstClients[0].stRecvQueue, &data);
    ASSERT_GT(size, 0);
    std::string recvData((char*)data, size);
    free(data);
    ASSERT_EQ(recvData, msg);
}
