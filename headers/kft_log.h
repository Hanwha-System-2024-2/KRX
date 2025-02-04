#include <stdio.h>
#include <stdarg.h>

#define RECERIVE_LOG_FILE "/home/ec2-user/KRX/log/receive_server.log"
#define MATCH_LOG_FILE "/home/ec2-user/KRX/log/match_server.log"
#define SEND_LOG_FILE "/home/ec2-user/KRX/log/send_server.log"

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
