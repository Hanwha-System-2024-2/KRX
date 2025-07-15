# 컴파일러
CC = gcc

# macOS + Homebrew MySQL 경로 (버전 독립)
MYSQL_PREFIX = /opt/homebrew/opt/mysql
MYSQL_INCLUDE = $(MYSQL_PREFIX)/include/mysql
MYSQL_LIB = $(MYSQL_PREFIX)/lib
ZSTD_LIB = /opt/homebrew/opt/zstd/lib
# OpenSSL
OPENSSL_LIB = /opt/homebrew/opt/openssl@3/lib
NCURSES_CFLAGS := $(shell pkg-config --cflags ncurses)
NCURSES_LIBS := $(shell pkg-config --libs ncurses)

# 공통 플래그
CFLAGS = -Wall -I$(MYSQL_INCLUDE) -I../headers
LDFLAGS = -L$(MYSQL_LIB) -L$(ZSTD_LIB) -L$(OPENSSL_LIB) -lmysqlclient -lz -lzstd -lssl -lcrypto -lresolv 

# 실행 파일 이름
TARGETS = kmt_server menu

# 소스 파일
KMT_SOURCES = kmt_server_db.c db_sql.c
MENU_SOURCES = hanwha_krx_screen.c db_sql.c

# 헤더 파일
HEADERS = ../headers/krx_messages.h ../headers/kmt_common.h

# 기본 빌드
all: $(TARGETS)

# kmt_server 빌드
kmt_server: $(KMT_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(KMT_SOURCES) -o $@ $(LDFLAGS)

# menu 빌드 (ncursesw 포함)
menu: $(MENU_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(NCURSES_CFLAGS) $(MENU_SOURCES) -o $@ $(LDFLAGS) $(NCURSES_LIBS)

# 정리
clean:
	rm -f $(TARGETS)

# 전체 재빌드
rebuild: clean all
