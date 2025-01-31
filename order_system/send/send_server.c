#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "../../headers/kft_log.h"
#include "../../headers/kft_ipc_db.h"

#define SERVER_IP "3.35.106.137"   // fep
#define SERVER_IP "43.201.218.194" // test server
#define SERVER_PORT 8089

// fep 서버에 연결
int connect_to_server() {
    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("소켓 생성 실패");
        log_message("ERROR", "ExecutionNotifier", "소켓 생성 실패");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        log_message("ERROR", "ExecutionNotifier", "IP 변환 실패");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_message("ERROR", "ExecutionNotifier", "서버 연결 실패");
        close(sock);
        return -1;
    }

    printf("서버(%s:%d) 연결 성공!\n", SERVER_IP, SERVER_PORT);
    log_message("NOTICE", "ExecutionNotifier", "서버 연결 성공 (%s:%d)", SERVER_IP, SERVER_PORT);

    return sock;
}

int main() {
    printf("체결 통지 프로세스 시작\n");
    log_message("NOTICE", "Server", "체결 통지 프로세스 시작.");

    // 메시지 큐 생성 (초기화)
    int execution_queue_id = init_message_queue(EXECUTION_QUEUE_ID);
    
    // 서버 연결
    int sock = connect_to_server();
    if (sock == -1) {
        log_message("ERROR", "Server", "서버 초기 연결 실패. 종료.");
        printf("서버 초기 연결 실패. 종료\n");
        return EXIT_FAILURE;
    }
    printf("서버 초기 연결 성공\n");

    
    kft_execution exec;
    while (1) {
        // 매칭 프로세스에서 보낸 체결 전문 수신 대기
        if (msgrcv(execution_queue_id, &exec, sizeof(kft_execution), 0, 0) >= 0) {
            // 체결 전문 수신
            printf("체결 완료: %s, 가격: %d\n", exec.transaction_code, exec.executed_price);
            log_message("INFO", "ExecutionProcessor", "거래 체결 정보 수신. 거래 코드: %s", exec.transaction_code);
            
            // fep 서버에 체결 전문 송신
            if (send(sock, &exec, sizeof(kft_execution), 0) < 0) {
                perror("체결 정보 전송 실패");
                log_message("ERROR", "Trace", "체결 정보 전송 실패. 재연결 시도.");
                // 전송 실패 시 재시도
                for (int i = 0; i < 3; i++) {
                    sock = connect_to_server();
                    if (sock != -1) {
                        log_message("NOTICE", "Trace", "서버 재연결 성공");
                        printf("서버 재연결 성공");
                        break;
                    }
                    sleep(2);
                }

                if (sock == -1) {
                    log_message("ERROR", "Trace", "서버 재연결 실패. 프로그램 종료");
                    exit(EXIT_FAILURE);
                }

                log_message("NOTICE", "ExecutionNotifier", "서버 재연결 성공.");
            } else {
                printf("체결 정보 전송 완료!\n");
                log_message("INFO", "ExecutionNotifier", "체결 정보 전송 완료. 거래 코드: %s", exec.transaction_code);
            }
        }
    }

    close(sock);
    return 0;
}
