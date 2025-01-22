#include "kft_common.h"
#include <time.h>

FILE *log_file = NULL;
void open_log_file() {
    if (log_file == NULL) {
        log_file = fopen("server.log", "a");  // 계속 추가 (append) 모드
        if (!log_file) {
            perror("로그 파일 열기 실패");
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
void log_message(const char *level, const char *module, const char *format,  ...) {
    if (!log_file) open_log_file();  // 파일이 닫혀 있으면 다시 열기

    if (!log_file) return;  // 파일 열기에 실패하면 리턴

    // 현재 시간 가져오기
    char timestamp[20];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    // 로그 메시지 작성
    va_list args;
    va_start(args, format);
    fprintf(log_file, "[%s] [%s] %s: ", timestamp, level, module);  // 시간과 로그 수준 출력
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