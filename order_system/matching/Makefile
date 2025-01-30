# 컴파일러 및 플래그
CC = gcc
CFLAGS = -Wall -Wextra -g -I../headers
LDFLAGS = -L/usr/lib64/mysql -lmysqlclient

# 실행 파일 이름
TARGET = kft_matching_server

# 소스 및 헤더 파일
SOURCES = matching_server.c ../db_sql.c ../kft_log.c ../kft_ipc.c
HEADERS = ../../headers/kft_log.h ../../headers/kft_ipc.h ../../headers/kft_db_sql.h ../../headers/krx_messages.h


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
