#ifndef KFT_COMMON_H
#define KFT_COMMON_H

#include <stdio.h>
#include <stdarg.h>

// 로그 파일 전역 변수
extern FILE *log_file;

// 로그 함수
void open_log_file();
void close_log_file();
void log_message(const char *level, const char *module, const char *format, ...);

// 시간 포맷 함수
void get_timestamp(char *timestamp);

#endif // KFT_COMMON_H
