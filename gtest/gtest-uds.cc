#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include "uds.h"

#define TEST_SOCKET_PATH "/tmp/test_uds_socket"
#define TEST_MESSAGE     "AcewaveTech UDS Test"
#define BUFFER_SIZE      128

class UDSTest : public ::testing::Test {
protected:
    int listen_sock;   ///< 리스닝 소켓 (서버에서 accept 전에 사용)
    int server_sock;   ///< 서버 측 클라이언트와의 통신용 소켓 (accept 결과)
    int client_sock;   ///< 클라이언트 소켓
    std::thread server_thread;
    char recv_buffer[BUFFER_SIZE];

    /**
     * @brief 테스트 초기화 함수
     */
    void SetUp() override {
        memset(recv_buffer, 0, sizeof(recv_buffer));

        // 서버 소켓 리스닝 준비
        listen_sock = createUdsServerSocket(TEST_SOCKET_PATH);
        ASSERT_GT(listen_sock, 0);

        // 클라이언트 연결을 수락하는 서버 쓰레드
        server_thread = std::thread([this]() {
            server_sock = accept(listen_sock, NULL, NULL);
            ASSERT_GT(server_sock, 0) << "accept 실패";
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 서버 준비 시간 대기

        // 클라이언트 연결
        client_sock = createUdsClientSocket(TEST_SOCKET_PATH);

        if (server_thread.joinable())
            server_thread.join();
    }

    /**
     * @brief 테스트 종료 함수
     */
    void TearDown() override {
        uds_close(client_sock);
        uds_close(server_sock);
        uds_close(listen_sock);
        unlink(TEST_SOCKET_PATH);
    }
};

/**
 * @brief UDS 송수신 테스트
 */
TEST_F(UDSTest, SendReceiveTest) {
    const char *msg = TEST_MESSAGE;

    int sent = udsSendMsg(client_sock, msg, strlen(msg));
    ASSERT_GT(sent, 0) << "메시지 전송 실패";

    int received = udsRecvMsg(server_sock, recv_buffer, sizeof(recv_buffer));
    ASSERT_GT(received, 0) << "메시지 수신 실패";

    recv_buffer[received] = '\0';
    ASSERT_STREQ(recv_buffer, msg) << "수신한 메시지가 전송한 메시지와 다릅니다.";
}

/**
 * @brief UDS 타임아웃 수신 테스트 - 데이터 없는 경우
 */
TEST_F(UDSTest, TimeoutReceiveTest_NoData) {
    int ret = udsRecvMsgTimeout(server_sock, recv_buffer, sizeof(recv_buffer), 500); // 500ms
    ASSERT_EQ(ret, 0) << "타임아웃이 발생하지 않았습니다.";
}

/**
 * @brief UDS 타임아웃 내 수신 테스트 - 데이터가 있는 경우
 */
TEST_F(UDSTest, TimeoutReceiveTest_WithData) {
    const char *msg = TEST_MESSAGE;

    std::thread sender([this, msg]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        udsSendMsg(client_sock, msg, strlen(msg));
    });

    int ret = udsRecvMsgTimeout(server_sock, recv_buffer, sizeof(recv_buffer), 500);
    ASSERT_GT(ret, 0) << "데이터를 수신하지 못했습니다.";

    recv_buffer[ret] = '\0';
    ASSERT_STREQ(recv_buffer, msg) << "수신된 메시지가 전송한 메시지와 일치하지 않습니다.";

    if (sender.joinable())
        sender.join();
}

/**
 * @brief 클라이언트 종료 후 서버에서 recv() 결과 확인
 */
TEST_F(UDSTest, ReceiveAfterClientClose) {
    uds_close(client_sock);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int received = udsRecvMsg(server_sock, recv_buffer, sizeof(recv_buffer));
    EXPECT_LE(received, 0) << "클라이언트 종료 후에도 데이터가 수신되었습니다.";
}

/**
 * @brief 여러 클라이언트가 동시에 서버에 연결하여 각각 메시지를 보내는 테스트
 *
 * 서버는 여러 클라이언트의 연결을 순차적으로 수락하고,
 * 각 클라이언트로부터 수신한 메시지가 예상한 것인지 확인합니다.
 */
#define CLIENT_COUNT 3
TEST(UdsMultiClientTest, MultiClientCommunicationWithAccept) {
    std::vector<std::thread> client_threads;
    std::vector<std::string> expected_messages;
    std::vector<int> accepted_socks;
    char buffer[BUFFER_SIZE] = {0};

    // 서버 생성 (accept 포함 X)
    int server_fd = createUdsServerSocket(TEST_SOCKET_PATH);
    ASSERT_GT(server_fd, 0);

    // 클라이언트 생성 및 메시지 전송 쓰레드
    for (int i = 0; i < CLIENT_COUNT; ++i) {
        std::string msg = "Client message " + std::to_string(i + 1);
        expected_messages.push_back(msg);

        client_threads.emplace_back([msg]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * (rand() % 3)));
            int sock = createUdsClientSocket(TEST_SOCKET_PATH);
            udsSendMsg(sock, msg.c_str(), msg.length());
            uds_close(sock);
        });
    }

    // 서버 측에서 클라이언트 accept 및 메시지 수신
    for (int i = 0; i < CLIENT_COUNT; ++i) {
        int client_sock = accept(server_fd, NULL, NULL);
        ASSERT_GT(client_sock, 0);
        accepted_socks.push_back(client_sock);

        memset(buffer, 0, sizeof(buffer));
        int len = udsRecvMsg(client_sock, buffer, sizeof(buffer) - 1);
        ASSERT_GT(len, 0);

        buffer[len] = '\0';
        EXPECT_NE(std::find(expected_messages.begin(), expected_messages.end(), std::string(buffer)), expected_messages.end()) << "예상하지 못한 메시지를 수신했습니다.";
    }

    // 자원 정리
    for (int s : accepted_socks) uds_close(s);
    uds_close(server_fd);
    unlink(TEST_SOCKET_PATH);

    for (auto &t : client_threads) {
        if (t.joinable()) t.join();
    }
}


