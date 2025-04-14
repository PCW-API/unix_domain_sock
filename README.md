# Unix Domain Socket (UDS) Communication Library

이 라이브러리는 **UNIX 도메인 소켓 (UDS)** 을 기반으로 하는 로컬 프로세스 간 통신 기능을 제공합니다.  
서버/클라이언트 구조의 소켓 통신을 쉽게 구현할 수 있도록 여러 유틸리티 함수들이 포함되어 있습니다.

---



## 파일 구성

```
.
├── uds.c            # UDS 함수 구현
├── uds.h            # UDS 함수 헤더
├── gtest-uds.cc     # GoogleTest 기반 단위 테스트
├── Makefile         # 빌드용 Makefile (옵션)
└── README.md        # 이 문서
```

---



## 제공 함수

| 함수                                               | 설명                                  |
| -------------------------------------------------- | ------------------------------------- |
| `createUdsServerSocket(path)`                      | UDS 서버 소켓 생성 및 리스닝          |
| `createUdsClientSocket(path)`                      | UDS 클라이언트 소켓 생성 및 서버 연결 |
| `udsSendMsg(sock, data, len)`                      | 데이터 전송 (`send()`)                |
| `udsRecvMsg(sock, buffer, len)`                    | 데이터 수신 (`recv()`)                |
| `udsRecvMsgTimeout(sock, buffer, len, timeout_ms)` | select 기반 타임아웃 수신             |
| `uds_close(sock)`                                  | 소켓 종료 (`close()`)                 |

---



## 예제

### 서버

```c
int listen_fd = createUdsServerSocket("/tmp/my_socket");
int client_fd = accept(listen_fd, NULL, NULL);

char buf[128];
int len = udsRecvMsg(client_fd, buf, sizeof(buf));
buf[len] = '\0';
printf("받은 메시지: %s\n", buf);

uds_close(client_fd);
uds_close(listen_fd);
unlink("/tmp/my_socket");
```

### 클라이언트

```c
int sock = createUdsClientSocket("/tmp/my_socket");
const char *msg = "Hello from client!";
udsSendMsg(sock, msg, strlen(msg));
uds_close(sock);
```

### 타임아웃 수신

```c
char buf[128];
int len = udsRecvMsgTimeout(sock, buf, sizeof(buf), 1000);  // 1000ms
if (len > 0) {
    buf[len] = '\0';
    printf("수신된 메시지: %s\n", buf);
} else if (len == 0) {
    printf("수신 타임아웃 발생\n");
}
```

---



## 테스트 실행 (Google Test)

단위 테스트는 `gtest-uds.cc`에 구현되어 있습니다.

### 빌드 & 실행

```bash
make gtest
./gtest-uds
```

테스트 항목:

- 메시지 송수신
- 수신 타임아웃
- 클라이언트 종료 시 예외처리
- 멀티 클라이언트 처리

---



## 참고 사항

- `/tmp` 디렉토리 등 소켓 파일 경로에 권한이 있어야 합니다.
- 소켓 생성 전 `unlink(path)` 호출로 중복 파일 제거 필요합니다.

---



## 라이선스

> MIT License  
> 자유롭게 사용, 수정, 배포 가능합니다.

---



## 작성자

박철우 @ Acewave Tech
