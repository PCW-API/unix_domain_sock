/**
 * @file uds-gtest.cc
 * @brief UDS 서버 테스트용 GoogleTest 스위트
 *
 * 본 파일은 UDS 서버의 클라이언트 연결, 데이터 송수신, 복수 클라이언트 처리 등
 * 주요 기능을 테스트하는 자동화 테스트 코드입니다.
 */
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

UDS_SERVER g_stUdsServer;

constexpr const char* TEST_SOCKET_PATH = "/tmp/test_socket"; ///< 테스트용 UDS 소켓 경로
constexpr int TEST_CLIENT_COUNT = 5;                          ///< 테스트 클라이언트 수
constexpr int CONNECT_WAIT_MS = 100;                          ///< 클라이언트 연결 대기 시간(ms)
constexpr int DATA_WAIT_MS = 300;                             ///< 데이터 수신 대기 시간(ms)

/**
 * @brief 테스트용 UDS 서버 초기화 함수
 *
 * 서버 소켓을 생성하고, 클라이언트 배열 및 뮤텍스를 초기화합니다.
 * 연결 관리, 송수신 스레드를 시작합니다.
 */
void startUdsServer() {    
    g_stUdsServer.iServerSock = createUdsServerSocket(TEST_SOCKET_PATH, TEST_CLIENT_COUNT);
    pthread_mutex_init(&g_stUdsServer.mutex, NULL);
    g_stUdsServer.iMaxClients = TEST_CLIENT_COUNT;
    g_stUdsServer.iRunning = 0;
    g_stUdsServer.iClientCount = 0;
    g_stUdsServer.pstClients = (CLIENT *)malloc(sizeof(CLIENT) * TEST_CLIENT_COUNT);
    for (int i = 0; i < g_stUdsServer.iMaxClients; ++i) {
        g_stUdsServer.pstClients[i].iSock = -1;
        g_stUdsServer.pstClients[i].iActive = 0;
        g_stUdsServer.pstClients[i].iId = -1;
    }
    std::thread(&connectionManagerThread, &g_stUdsServer).detach();
    std::thread(&sendThread, &g_stUdsServer).detach();
    std::thread(&recvThread, &g_stUdsServer).detach();    
}

/**
 * @brief 테스트 종료 후 서버 정리 함수
 *
 * 서버 실행을 중단하고 소켓을 종료합니다.
 */
void stopUdsServer() {
    g_stUdsServer.iRunning = 0;
    g_stUdsServer.iClientCount = 0;
    // g_stUdsServer.pstClients
    if (g_stUdsServer.iServerSock) {
        udsClose(g_stUdsServer.iServerSock);
    }
}

/**
 * @brief 테스트 클라이언트 소켓 생성 함수
 * @return 유효한 클라이언트 소켓 디스크립터
 */
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

/**
 * @class UdsServerTest
 * @brief 공통 테스트 픽스처 클래스
 *
 * 테스트 전후에 서버를 초기화하고 정리하며,
 * 클라이언트 소켓 및 스레드 관리를 수행합니다.
 */
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
        for (int sock : clientSockets) {
            if (sock > 0) shutdown(sock, SHUT_RDWR); // recv() 깨우기
        }
        for (auto& t : clientThreads) 
            if (t.joinable()) 
                t.join();

        for (int sock : clientSockets) 
            if (sock > 0) 
                close(sock);
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
            if (g_stUdsServer.pstClients[i].iActive) {
                void* data = nullptr;
                int size = queuePop(&g_stUdsServer.pstClients[i].stRecvQueue, &data);
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
            if (len > 0) 
                send(sock, buf, len, 0);
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
#if 1
/**
 * @test ClientConnectDisconnectTest
 * @brief 클라이언트 연결 및 해제 테스트
 *
 * 여러 클라이언트를 생성하여 서버에 연결하고, 순차적으로 연결을 종료한 뒤
 * 서버의 클라이언트 카운트가 0이 되는지를 확인합니다.
 */
TEST_F(UdsServerTest, ClientConnectDisconnectTest) {
    createClients(TEST_CLIENT_COUNT);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ASSERT_EQ(g_stUdsServer.iClientCount, TEST_CLIENT_COUNT);
    for (size_t i = 0; i < clientSockets.size(); ++i) {        
        close(clientSockets[i]);
        clientSockets[i] = -1;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ASSERT_EQ(g_stUdsServer.iClientCount, 0);
}

/**
 * @test ClientSendDataTest
 * @brief 클라이언트 → 서버 데이터 송신 테스트
 *
 * 각 클라이언트가 데이터를 서버에 송신하고,
 * 서버 측 수신 큐에서 해당 데이터를 올바르게 수신했는지를 검증합니다.
 */
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

/**
 * @test ServerClientLoopback
 * @brief 서버 → 클라이언트 루프백 테스트
 *
 * 각 클라이언트가 서버로부터 받은 데이터를 다시 서버에 반환하도록 설정하여,
 * 송신 큐를 통해 전송된 데이터가 정상적으로 수신되었는지 확인합니다.
 */
TEST_F(UdsServerTest, ServerClientLoopback) {
    createClientsWithLoopback(TEST_CLIENT_COUNT);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
        if (g_stUdsServer.pstClients[i].iActive) {
            std::string msg = "ServerData_" + std::to_string(i);
            testData.push_back(msg);
            char* data = strdup(msg.c_str());
            queuePush(&g_stUdsServer.pstClients[i].stSendQueue, data, msg.size());
        }
    }
    collectReceivedData();
    int matched = 0;
    for (const auto& expected : testData) {
        for (const auto& actual : receivedData)
            if (expected == actual) 
                matched++;
    }    
    ASSERT_EQ(matched, TEST_CLIENT_COUNT);
}

/**
 * @test ClientUnexpectedTermination
 * @brief 비정상 종료된 클라이언트 처리 테스트
 *
 * fork()를 사용해 클라이언트를 생성하고,
 * 종료 시 close() 없이 바로 종료하여 서버가 해당 연결을 적절히 해제하는지 검증합니다.
 */
TEST_F(UdsServerTest, ClientUnexpectedTermination) {
    pid_t pid = fork();
    if (pid == 0) {
        int sock = createTestClientSocket();
        (void)sock;
        _exit(0); // child exits without close
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        ASSERT_LE(g_stUdsServer.iClientCount, TEST_CLIENT_COUNT - 1);
    }
}


/**
 * @test FragmentedSendTest
 * @brief 분할 전송 테스트
 *
 * 메시지를 두 부분으로 나누어 순차적으로 전송하여,
 * 서버가 이를 하나의 연속된 메시지로 수신하는지를 확인합니다.
 */
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
    int size = queuePop(&g_stUdsServer.pstClients[0].stRecvQueue, &data);
    ASSERT_GT(size, 0);
    std::string msg((char*)data, size);
    free(data);

    EXPECT_TRUE(msg.find("Part1_") != std::string::npos);
    EXPECT_TRUE(msg.find("Part2_END") != std::string::npos);
}

/**
 * @test SimultaneousClientSendTest
 * @brief 동시 다중 클라이언트 송신 테스트
 *
 * 여러 클라이언트가 동시에 메시지를 송신하고,
 * 서버가 모든 메시지를 정상적으로 수신했는지 확인합니다.
 */
TEST_F(UdsServerTest, SimultaneousClientSendTest) {
    createClients(TEST_CLIENT_COUNT);
    std::vector<std::thread> senders;
    for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
        int sock = clientSockets[i];  // 명시적으로 변수 선언
        senders.emplace_back([sock, i]() {
            std::string msg = "Simultaneous_Client_" + std::to_string(i);
            send(sock, msg.c_str(), msg.size(), 0);
        });
    }
    for (auto& t : senders) t.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(DATA_WAIT_MS));
    int received = 0;
    for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
        void* data = nullptr;
        int size = queuePop(&g_stUdsServer.pstClients[i].stRecvQueue, &data);
        if (size > 0 && data) {
            received++;
            free(data);
        }
    }
    ASSERT_EQ(received, TEST_CLIENT_COUNT);
}

/**
 * @test SendQueueSaturationTest
 * @brief 송신 큐 포화 테스트
 *
 * 하나의 클라이언트에 다량의 메시지를 push하여,
 * 서버가 송신 큐를 과부하 상태에서도 안정적으로 처리하는지 확인합니다.
 */
TEST_F(UdsServerTest, SendQueueSaturationTest) {
    int sock = createTestClientSocket();
    ASSERT_GT(sock, 0);
    clientSockets.push_back(sock);

    for (int i = 0; i < 1000; ++i) {
        std::string msg = "FloodData_" + std::to_string(i);
        char* data = strdup(msg.c_str());
        queuePush(&g_stUdsServer.pstClients[0].stSendQueue, data, msg.size());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // No crash or memory issue means test passed; optionally validate with metrics
    SUCCEED();
}

/**
 * @test ClientReconnectTest
 * @brief 클라이언트 재접속 테스트
 *
 * 클라이언트가 연결 후 종료되고, 이후 동일 소켓 경로로 재접속하여
 * 송수신이 정상적으로 이루어지는지를 검증합니다.
 */
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
    int size = queuePop(&g_stUdsServer.pstClients[0].stRecvQueue, &data);
    ASSERT_GT(size, 0);
    std::string recvData((char*)data, size);
    free(data);
    ASSERT_EQ(recvData, msg);
}
#endif

/**
 * @test MultiClientHighVolumeSendTest
 * @brief 고속 대용량 멀티 클라이언트 송신 테스트
 *
 * 여러 클라이언트가 각각 일정 시간 간격으로 다량의 메시지를 지속적으로 송신하고,  
 * 서버가 모든 메시지를 정확하게 수신하여 누락 없이 처리하는지를 검증합니다.
 *
 * 테스트 절차:
 * 1. 클라이언트 5개를 생성하여 서버에 연결
 * 2. 각 클라이언트는 200개의 메시지를 전송
 * 3. 서버는 실시간으로 클라이언트 수신 큐에서 메시지를 수집
 * 4. 예상 메시지와 수신 메시지를 비교하여 누락 여부를 확인
 */
#define SEND_DATA_COUNT 200
void clientContinousSendDataThread(int sock) {
    char buf[64];
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    for (int i = 0; i < SEND_DATA_COUNT; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        snprintf(buf, sizeof(buf), "client_%d_%03d", sock, i);
        send(sock, buf, strlen(buf), 0);        
    }
}

TEST_F(UdsServerTest, MultiClientHighVolumeSendTest) {
    createClients(TEST_CLIENT_COUNT);

    //expectedMessages 미리 생성
    std::set<std::string> expectedMessages;
    for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
        int sock = clientSockets[i];
        for (int j = 0; j < SEND_DATA_COUNT; ++j) {
            char buf[64];
            snprintf(buf, sizeof(buf), "client_%d_%03d", sock, j);
            expectedMessages.insert(buf);
        }
    }

    //각 클라이언트에서 데이터 전송
    std::vector<std::thread> clientThreads;
    for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
        int sock = clientSockets[i];
        clientThreads.emplace_back(clientContinousSendDataThread, sock);
    }

    //서버가 실시간으로 수신 데이터를 수집
    std::set<std::string> actualMessages;
    int totalReceived = 0;
    constexpr int TOTAL_EXPECTED = (SEND_DATA_COUNT*5);
    auto start = std::chrono::steady_clock::now();
    while (totalReceived < TOTAL_EXPECTED) {
        for (int i = 0; i < TEST_CLIENT_COUNT; ++i) {
            void* data = nullptr;
            int size = queuePop(&g_stUdsServer.pstClients[i].stRecvQueue, &data);
            if (size > 0 && data) {
                std::string msg((char*)data, size);

                // ✔ 메시지 정제 처리 추가
                msg.erase(std::remove_if(msg.begin(), msg.end(), [](char c) {
                    return c == '\n' || c == '\r' || c == '\0';
                }), msg.end());

                actualMessages.insert(msg);
                free(data);
                totalReceived++;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
            break;
        }
    }

    for (auto& t : clientThreads) t.join();

    //검증
    EXPECT_EQ(actualMessages.size(), expectedMessages.size());
    for (const auto& msg : expectedMessages) {
        EXPECT_TRUE(actualMessages.count(msg)) << "Missing: " << msg <<", size is "<<msg.size();
    }
}



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}