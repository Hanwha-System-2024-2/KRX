#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../../headers/kft_log.h"
#include "../../headers/kft_ipc_db.h"


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>


#define PORT 8081
#define ORDER_QUEUE_ID 0001
#define BUFFER_SIZE 4096

void process_order(int client_socket, fkq_order *order, int que_id) {
    kft_order response;

    // 전문 프로토콜 유효성 검증
    if(order->header.tr_id !=FKQ_ORDER){
        printf("[WARNING] 잘못된 요청. 전문 ID: %d\n", order->header.tr_id);
        log_message("WARNING", "OrderProcessor", "잘못된 요청. 전문 ID: %d", order->header.tr_id);

        response.header.tr_id=KFT_ORDER;
        response.header.length = sizeof(kft_order);
        strcpy(response.transaction_code, order->transaction_code);
        response.transaction_code[sizeof(response.transaction_code) - 1] = '\0';
        strcpy(response.user_id, order->user_id);
        response.user_id[sizeof(response.user_id) - 1] = '\0';
        get_timestamp(response.time);
        strcpy(response.reject_code, "E002");
        response.reject_code[sizeof(response.reject_code) - 1] = '\0';

        send(client_socket, &response, sizeof(response), 0);

        return;
    }
    // 전문 길이 검증
    if (order->header.length != sizeof(fkq_order)) {
        printf("[INFO] 주문 수신 - 종목: %s, 거래코드: %s, 유저: %s, 수량: %d, 가격: %d\n",
           order->stock_code, order->transaction_code, order->user_id, order->quantity, order->price);
        log_message("WARNING", "OrderProcessor", "잘못된 전문 길이: %d (예상: %lu)", order->header.length, sizeof(fkq_order));

        
        response.header.tr_id=KFT_ORDER;
        response.header.length = sizeof(kft_order);
        strcpy(response.transaction_code, order->transaction_code);
        response.transaction_code[sizeof(response.transaction_code) - 1] = '\0';
        strcpy(response.user_id, order->user_id);
        response.user_id[sizeof(response.user_id) - 1] = '\0';
        get_timestamp(response.time);
        strcpy(response.reject_code, "E001");
        response.reject_code[sizeof(response.reject_code) - 1] = '\0';

        send(client_socket, &response, sizeof(response), 0);
        return;
    }

    // 주문 요청 수신 완료
    printf("[INFO] 주문 요청 수신 - 종목: %s %s, 거래코드: %s, 유저: %s, 유형: %c, 수량: %d, 시간: %s, 지정가: %d, 원주문번호: %s\n",
        order->stock_code, order->stock_name, order->transaction_code, order->user_id, order->order_type, order->quantity, order->order_time, order->price, order->original_order);
    log_message("INFO", "OrderProcessor", "주문 수신 - 종목: %s, 거래코드: %s, 유저: %s, 수량: %d, 가격: %d",
        order->stock_code, order->transaction_code, order->user_id, order->quantity, order->price);

    // 주문 응답 전송
    response.header.tr_id=KFT_ORDER;
    response.header.length = sizeof(kft_order);
    strcpy(response.transaction_code, order->transaction_code);
    response.transaction_code[sizeof(response.transaction_code) - 1] = '\0';
    strcpy(response.user_id, order->user_id);
    response.user_id[sizeof(response.user_id) - 1] = '\0';
    get_timestamp(response.time);
    strcpy(response.reject_code, "0000");
    response.reject_code[sizeof(response.reject_code) - 1] = '\0';

    send(client_socket, &response, sizeof(response), 0);


    // 메시지 큐에 주문 추가 (매칭 프로세스로 전달)
    send_order_to_queue(que_id, order);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;                      // 서버, 클라이언트의 IP 주소 및 포트 정보를 설정
    socklen_t addrlen = sizeof(address);


    int que_id= init_message_queue(ORDER_QUEUE_ID);

    open_log_file();

    log_message("NOTICE", "Server", "주문 수신 서버 시작. 포트: %d", PORT);
    printf("[NOTICE] 주문 수신 서버 시작. 포트: %d\n", PORT);

    // 소켓 생성
    server_fd = socket(AF_INET, SOCK_STREAM, 0);     // IPv4 기반, TCP 소켓, 자동으로 적절한 프로토콜 사용
    if (server_fd == -1) {
        perror("[ERROR] 소켓 생성 실패");
        log_message("ERROR", "Server", "소켓 생성 실패");
        exit(EXIT_FAILURE);
    }
    printf("[NOTICE] 소켓 생성 성공. 소켓 번호: %d\n", server_fd);
    log_message("NOTICE", "Server", "소켓 생성 성공. 소켓 번호: %d", server_fd);
    

    address.sin_family = AF_INET;                    // IPv4 주소 체계
    address.sin_addr.s_addr = INADDR_ANY;            // 모든 네트워크 인터페이스에서 연결을 수락
    address.sin_port = htons(PORT);                  // 포트를 네트워크 바이트 순서로 변환하여 설정

    // 바인딩: 소켓과 포트를 연결
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[ERROR] 바인딩 실패");
        log_message("ERROR", "Server", "바인딩 실패");
        exit(EXIT_FAILURE);
    }

    // 리스닝 (클라이언트 1명)
    if (listen(server_fd, 1) < 0) {
        perror("[ERROR] 리스닝 실패");
        log_message("ERROR", "Server", "리스닝 실패");
        exit(EXIT_FAILURE);
    }

    printf("[NOTICE] 거래소 주문 응답 서버 시작. 포트: %d\n", PORT);
    log_message("NOTICE", "Server", "클라이언트 연결 대기 중...");

    while (1) {
    // 클라이언트 연결 대기 (한 번만 실행)
        client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_socket < 0) {
            perror("[ERROR] 클라이언트 연결 실패");
            log_message("ERROR", "Server", "클라이언트 연결 실패");
            // exit(EXIT_FAILURE);
            continue;
        }
        printf("[NOTICE] 클라이언트 연결됨. 소켓 번호: %d\n", client_socket);
        log_message("NOTICE", "Server", "클라이언트 연결됨. 소켓 번호: %d",client_socket);

        // 클라이언트와 통신
        while (1) {
            fkq_order order;
            // int bytes_read = recv(client_socket, &order, sizeof(order), 0);

            char recv_buffer[BUFFER_SIZE];  // 데이터를 저장할 버퍼

            int bytes_read = recv(client_socket, recv_buffer, 4096, 0);

            if (bytes_read <= 0) {  // 클라이언트 종료 감지
                printf("[NOTICE] 클라이언트 연결 종료\n");
                log_message("NOTICE", "Server", "클라이언트 연결 종료");
                close(client_socket);
                break; // 내부 루프 종료 후 다시 accept() 대기
            }

            if (bytes_read > 0) {
                // recv_buffer에서 order 크기만큼 데이터를 잘라서 사용
                int num_orders = bytes_read / sizeof(order);
                for (int i = 0; i < num_orders; i++) {
                    memcpy(&order, recv_buffer + (i * sizeof(order)), sizeof(order));
                    process_order(client_socket, &order, que_id); // order 처리 함수
                }
            }


        }

    }

    // 마지막에만 소켓 닫기
    close(client_socket);
    close(server_fd);
    log_message("NOTICE", "Server", "소켓 close");

    close_log_file();
    return 0;
}

