# Unix Domain Socket Server Library (UDS Server)

UDS(Unix Domain Socket)를 기반으로 한 **멀티 클라이언트 서버 라이브러리**입니다.  

다중 클라이언트 연결, 송수신 처리, 스레드 기반 동작, 큐 버퍼 등을 포함하며  Google Test 기반 테스트 환경도 제공됩니다.



---



## 📁 파일 구성

```
include/
├── uds.h 					# UDS API 및 클라이언트/서버 구조 정의
├── uds-server.h 			# 서버 동작 정의 및 스레드 함수 선언
src/
├── uds.c 					# UDS 서버 소켓 및 클라이언트 생성 로직
├── connection-manager.c 	# 클라이언트 연결 관리 스레드
├── receiver.c 				# 클라이언트 수신 처리 스레드
├── sender.c 				# 클라이언트 송신 처리 스레드
gtest/
├── uds-gtest.cc 			# Google Test 기반 자동화 테스트 코드
Makefile 					# 라이브러리 및 테스트 빌드용 Makefile
```

---



---

## 🔧 빌드 방법

### 1. 라이브러리 빌드

```bash
make desktop       # libuds_desktop.so 생성
```

생성 결과 libuds_desktop.so 공유라이브러리 



### 2. 구글테스트 빌드 및 실행

```bash
make gtest
./uds-gtest
```

생성 결과 uds-gtest 실행 파일



---

## 📌 Makefile 주요 타겟

| 명령어         | 설명                                  |
| -------------- | ------------------------------------- |
| `make`         | 라이브러리 빌드 (`libuds_desktop.so`) |
| `make gtest`   | GoogleTest 기반 테스트 빌드           |
| `make clean`   | 빌드된 파일 정리                      |
| `make install` | `/usr/lib` 및 `/usr/include`에 설치   |



------

## 📦 설치 파일 경로

설치 시 다음 경로에 배치됩니다:

- `/usr/lib/libuds_desktop.so`
- `/usr/include/uds.h`
- `/usr/include/uds-server.h`



------

## 🧪 의존성

- POSIX Thread (`pthread`)
- GoogleTest (테스트용)
- queue 라이브러리 (`libqueue_desktop.so` 필요 시)



------

## 📄 라이선스

누구나 필요하시다면 사용하세요.


------

## ✍️ 작성자

- **박철우 (Cheolwoo Park)**
- Embedded Systems / Signal Processing
- GitHub: [PCW-Learning](https://github.com/PCW-Learning)
