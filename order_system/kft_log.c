#include "../headers/kft_log.h"
#include <time.h>

#define RECERIVE_LOG_FILE "/home/ec2-user/KRX/log/receive_server.log"
#define MATCH_LOG_FILE "/home/ec2-user/KRX/log/match_server.log"
#define SEND_LOG_FILE "/home/ec2-user/KRX/log/send_server.log"

FILE *log_file = NULL;
char *log_file_path = NULL;
void open_log_file() {
    if (log_file == NULL) {
        log_file = fopen(log_file_path, "a");  // 계속 추가 (append) 모드
        if (!log_file) {
            perror("[ERROR] 로그 파일 열기 실패");
            return;
        }
    }
}

void close_log_file() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

// 로그 기록 함수
void log_message(const char *level, const char *format,  ...) {
    if (!log_file) open_log_file();  // 파일이 닫혀 있으면 다시 열기

    if (!log_file) return;  // 파일 열기에 실패하면 리턴

    // 로그 메시지 작성
    va_list args;
    va_start(args, format);
    fprintf(log_file, "[%s] ", level); 
    vfprintf(log_file, format, args);                  // 가변 인자 출력
    fprintf(log_file, "\n");                           // 줄바꿈 추가
    va_end(args);

    fflush(log_file);  // 파일을 즉시 저장 (버퍼링 방지)
}




// 현재 시간 문자열 생성 함수
void get_timestamp(char *timestamp) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, 15, "%Y%m%d%H%M%S", tm_info);
}

char *get_timestamp_char() {
    static char timestamp[20]; 
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    return timestamp;
}

char *get_timestamp_char_long() {
    static char timestamp[15]; 
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", tm_info);
    return timestamp;
}