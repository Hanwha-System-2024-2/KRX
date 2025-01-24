# 컴파일러 및 플래그
CC = gcc
CFLAGS = -Wall -Wextra -g -I../headers
LDFLAGS = -I/usr/include/mysql -L/usr/lib64/mysql -lmysqlclient

# 실행 파일 이름
TARGETS = kmt_server menu

# 소스 파일 및 헤더 파일 경로
KMT_SOURCES = kmt_server_db.c db_sql.c
MENU_SOURCES = hanwha_krx_screen.c db_sql.c
HEADERS = ../headers/kmt_messages.h ../headers/kmt_common.h

# 기본 빌드 타겟
all: $(TARGETS)

# kmt_server 실행 파일 생성 규칙
kmt_server: $(KMT_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(KMT_SOURCES) -o $@ $(LDFLAGS)

# menu 실행 파일 생성 규칙
menu: $(MENU_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(MENU_SOURCES) -o $@ $(LDFLAGS) -lncursesw

# 정리(clean) 규칙
clean:
	rm -f $(TARGETS)

# 재빌드 규칙
rebuild: clean all
