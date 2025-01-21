# 컴파일러 및 플래그
CC = gcc
CFLAGS = -Wall -Wextra -g -I../headers
LDFLAGS = -L/usr/lib/x86_64-linux-gnu -lmysqlclient

# 실행 파일 이름
TARGET = kmt_server

# 소스 및 헤더 파일
SOURCES = kmt_server2.c db_sql.c
HEADERS = ../headers/kmt_messages.h ../headers/kmt_common.h

# 오브젝트 파일 생성 규칙
OBJECTS = $(SOURCES:.c=.o)

# 기본 빌드 타겟
all: $(TARGET)

# 실행 파일 생성 규칙
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# 개별 소스 파일의 오브젝트 파일 생성 규칙
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# 정리(clean) 규칙
clean:
	rm -f *.o $(TARGET)

# 재빌드 규칙
rebuild: clean all

