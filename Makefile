# 파일 경로 설정
SRC_DIR = src
INCLUDE_DIR = include
SOCKET_SRCS = $(wildcard $(SRC_DIR)/*.c)
SOCKET_OBJS = $(patsubst %.c, %.o, $(SOCKET_SRCS))
CFLAGS = -Wall -g -fPIC -I$(INCLUDE_DIR)

# 데스크탑용 설정
DESKTOP_TARGET_LIB = libuds_desktop.so
DESKTOP_CFLAGS = -Wall -g -fPIC -I$(INCLUDE_DIR)
DESKTOP_CC = gcc

# 구글테스트 관련 설정
GTEST_DIR = ../googletest
GTEST_INCLUDE_DIR = $(GTEST_DIR)/googletest/include
GTEST_LIB_DIR = $(GTEST_DIR)/build/lib

FOR_GTEST_SRCS = $(wildcard $(SRC_DIR)/*.c)
FOR_GTEST_OBJS = $(patsubst %.c, %_gtest.o, $(FOR_GTEST_SRCS))
MY_GTEST_DIR = gtest
MY_GTEST_SRCS = $(MY_GTEST_DIR)/gtest-uds.cc
MY_GTEST_OBJS = $(patsubst %.cc, %.o, $(MY_GTEST_SRCS))
GTEST_TARGET = gtest-uds

# 변수 정의
CC = gcc
CXX = g++
GTEST_CFLAGS = -Wall -g -I$(INCLUDE_DIR) -I$(GTEST_INCLUDE_DIR) -std=c++11
GTEST_LDFLAGS = -L$(GTEST_LIB_DIR) -lgtest -lgtest_main -lpthread

# 라이브러리 파일명
TARGET_LIB = libuds.so.1.0.0
SONAME = libuds.so.1
LINKNAME = libuds.so


# 기본 타겟
all: $(TARGET_LIB) symlinks

# 공유 라이브러리 생성
$(TARGET_LIB): $(SOCKET_OBJS)
	$(CC) -shared $(LDFLAGS) -Wl,-soname,$(SONAME) -o $@ $^

# 심볼릭 링크 생성
symlinks:
	ln -sf $(TARGET_LIB) $(SONAME)
	ln -sf $(SONAME) $(LINKNAME)

# desktop 타겟
desktop: $(SOCKET_SRCS)
	$(DESKTOP_CC) -shared $(DESKTOP_CFLAGS) -o $(DESKTOP_TARGET_LIB) $(SOCKET_SRCS)

# 구글테스트 빌드 및 실행
gtest: $(MY_GTEST_OBJS) $(FOR_GTEST_OBJS)
	$(CXX) $(GTEST_CFLAGS) -o $(GTEST_TARGET) $(MY_GTEST_OBJS) $(FOR_GTEST_OBJS) $(GTEST_LDFLAGS)
	
# 패턴 규칙: .c 파일을 .o 파일로 컴파일 (일반 빌드)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 패턴 규칙: .c 파일을 .gtest.o 파일로 컴파일 (구글테스트 빌드)
%_gtest.o: %.c
	$(CXX) $(GTEST_CFLAGS) -c $< -o $@

# 패턴 규칙: .cc 파일을 .o 파일로 컴파일 (구글테스트)
%.o: %.cc
	$(CXX) $(GTEST_CFLAGS) -c $< -o $@

# clean 타겟: 빌드 파일 정리
.PHONY: clean
clean:
	rm -f $(SOCKET_OBJS) $(TARGET_LIB) $(SONAME) $(LINKNAME) \
	      $(FOR_GTEST_OBJS) $(MY_GTEST_OBJS) $(GTEST_TARGET) \
		  $(DESKTOP_TARGET_LIB)