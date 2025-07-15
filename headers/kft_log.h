#include <stdio.h>
#include <stdarg.h>

#define RECERIVE_LOG_FILE "../log/receive_server.log"
#define MATCH_LOG_FILE "../log/match_server.log"
#define SEND_LOG_FILE "../log/send_server.log"
#define UPDATE_MARKET_LOG_FILE "../log/update_market.log"

// 로그 파일 전역 변수
extern FILE *log_file;
extern char *log_file_path;

// 로그 함수
void open_log_file(); 
void close_log_file();
void log_message(const char *level, const char *format,  ...);

// 시간 포맷 함수
void get_timestamp(char *timestamp);
char *get_timestamp_char();
char *get_timestamp_char_long();
