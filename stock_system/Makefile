# 컴파일러 및 플래그
CC = gcc
CFLAGS = -Wall -Wextra -g -I../headers
# LDFLAGS = -L/usr/lib/x86_64-linux-gnu -lmysqlclient
LDFLAGS = -I/usr/include/mysql -L/usr/lib64/mysql -lmysqlclient
# 실행 파일 이름
TARGET = kmt_server

# 소스 파일 및 헤더 파일 경로
SOURCES = kmt_server_db.c db_sql.c
HEADERS = ../headers/kmt_messages.h ../headers/kmt_common.h

# 기본 빌드 타겟
all: $(TARGET)

# 실행 파일 생성 규칙 (중간 .o 파일 없이)
$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

# 정리(clean) 규칙
clean:
	rm -f $(TARGET)

# 재빌드 규칙
rebuild: clean all
