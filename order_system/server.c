#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../headers/kft_common.h"
#include "../headers/kft_db_sql.h"
#include "../headers/kft_ipc.h"

#define PORT 8081

// 주문 요청 처리 함수
void process_order(int client_socket, fkq_order *order, MYSQL *conn) {
    kft_order response;
    
    if(order->header.tr_id !=FKQ_ORDER){
        printf("잘못된 요청입니다. 전문 id : %d\n", order->header.tr_id);
        log_message("WARNING", "OrderProcessor", "잘못된 요청. 전문 ID: %d", order->header.tr_id);
        return;
    }
    // 길이 검증
    if (order->header.length != sizeof(fkq_order)) {
        printf("잘못된 전문 길이: %d (예상: %lu)\n", order->header.length, sizeof(fkq_order));
        log_message("WARNING", "OrderProcessor", "잘못된 전문 길이: %d (예상: %lu)", order->header.length, sizeof(fkq_order));

        
        response.header.tr_id=KFT_ORDER;
        response.header.length = sizeof(kft_order);
        strcpy(response.transaction_code, order->transaction_code);
        strcpy(response.user_id, order->user_id);
        get_timestamp(response.time);
        strcpy(response.reject_code, "E001");

        send(client_socket, &response, sizeof(response), 0);
        return;
    }

    // 이미 있는 거래 코드인 경우 오류
   

    printf("주문 요청 수신 - 종목: %s %s, 거래코드: %s, 유저: %s, 유형: %c, 수량: %d, 시간: %s, 지정가: %d, 원주문번호: %s\n",
           order->stock_code, order->stock_name, order->transaction_code, order->user_id, order->order_type, order->quantity, order->order_time, order->price, order->original_order);
    
    log_message("INFO", "OrderProcessor", "주문 수신 - 종목: %s, 거래코드: %s, 유저: %s, 수량: %d, 가격: %d",
                order->stock_code, order->transaction_code, order->user_id, order->quantity, order->price);


    insert_order(conn, order);

    // 주문 응답 전송
    
    response.header.tr_id=KFT_ORDER;
    response.header.length = sizeof(kft_order);
    strcpy(response.transaction_code, order->transaction_code);
    strcpy(response.user_id, order->user_id);
    get_timestamp(response.time);
    strcpy(response.reject_code, "0000");

    send(client_socket, &response, sizeof(response), 0);


    // 오류인 경우 파악해 추가 (구매 가능 수량 이상을 요청한 경우, 상한가 초과 하한가 미만의 호가 요청)

    // 체결 여부 결정 (거래 코드 뒷자리가 3 또는 7이면 미체결)
    if (order->transaction_code[5] == '3' || order->transaction_code[5] == '7') {
        printf("미체결 처리. 거래 코드: %s\n", order->transaction_code);
        log_message("INFO", "OrderProcessor", "미체결 주문. 거래 코드: %s", order->transaction_code);
        
        // 시세 프로세스에 미체결된 주문 전달
		send_to_queue( 2, order->stock_code, order-> order_type,  order->price, order->quantity);
		
        return;
    }

    kft_execution execution;
    
    execution.header.tr_id=KFT_EXECUTION;
    execution.header.length = sizeof(kft_execution);
    strcpy(execution.transaction_code, order->transaction_code);
    execution.status_code = 0;
    get_timestamp(execution.time);
    execution.executed_price = order->price;
    strcpy(execution.original_order, order->original_order);
    strcpy(execution.reject_code, "0000");
    
    insert_execution(conn, &execution, order->quantity);
    

    // 응답 전송
    send(client_socket, &execution, sizeof(execution), 0);

    log_message("INFO", "OrderProcessor", "거래 체결. 거래 코드: %s", order->transaction_code);
        
    // 시세 프로세스에 체결된 주문 정보 전달
    send_to_queue( 1, execution.stock_code, order-> order_type,  execution.executed_price,order->quantity);

    
}


int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;                      // 서버, 클라이언트의 IP 주소 및 포트 정보를 설정
    socklen_t addrlen = sizeof(address);

    MYSQL *conn = connect_to_mysql();

    init_message_queue();  // 메시지 큐 초기화

    open_log_file();

    log_message("NOTICE", "Server", "서버 시작. 포트: %d", PORT);

    // 소켓 생성
    server_fd = socket(AF_INET, SOCK_STREAM, 0);     // IPv4 기반, TCP 소켓, 자동으로 적절한 프로토콜 사용
    if (server_fd == -1) {
        perror("소켓 생성 실패");
        log_message("ERROR", "Server", "소켓 생성 실패");
        exit(EXIT_FAILURE);
    }
    printf("소켓 생성 성공. 소켓 번호: %d\n", server_fd);
    log_message("NOTICE", "Server", "소켓 생성 성공. 소켓 번호: %d", server_fd);
    

    address.sin_family = AF_INET;                    // IPv4 주소 체계
    address.sin_addr.s_addr = INADDR_ANY;            // 모든 네트워크 인터페이스에서 연결을 수락
    address.sin_port = htons(PORT);                  // 포트를 네트워크 바이트 순서로 변환하여 설정

    // 바인딩: 소켓과 포트를 연결
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("바인딩 실패");
        log_message("ERROR", "Server", "바인딩 실패");
        exit(EXIT_FAILURE);
    }

    // 리스닝 (클라이언트 1명)
    if (listen(server_fd, 1) < 0) {
        perror("리스닝 실패");
        log_message("ERROR", "Server", "리스닝 실패");
        exit(EXIT_FAILURE);
    }

    printf("거래소 서버 시작. 포트: %d\n", PORT);
    log_message("NOTICE", "Server", "클라이언트 연결 대기 중...");

    // 클라이언트 연결 대기 (한 번만 실행)
    client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (client_socket < 0) {
        perror("클라이언트 연결 실패");
        log_message("ERROR", "Server", "클라이언트 연결 실패");
        exit(EXIT_FAILURE);
    }
    printf("클라이언트 연결됨. 소켓 번호: %d\n", client_socket);
    log_message("NOTICE", "Server", "클라이언트 연결됨. 소켓 번호: %d",client_socket);

    // 클라이언트와 통신
    while (1) {
        fkq_order order;
        int bytes_read = recv(client_socket, &order, sizeof(order), 0);

        if (bytes_read <= 0) {  // 클라이언트 종료 감지
            printf("클라이언트 연결 종료\n");
            log_message("NOTICE", "Server", "클라이언트 연결 종료");
            break;
        }
        process_order(client_socket, &order, conn);
    }


    // 마지막에만 소켓 닫기
    close(client_socket);
    close(server_fd);
    log_message("NOTICE", "Server", "소켓 close");

    close_log_file();
    return 0;
}
