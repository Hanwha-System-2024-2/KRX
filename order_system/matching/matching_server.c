#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../headers/kft_log.h"
#include "../../headers/kft_ipc.h"


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX_STOCKS 4

typedef struct {
    char stock_code[7];
    int closing_price;
} stock_info;

// 종목 정보
stock_info stock_list[MAX_STOCKS] = {
    {"000660", 0},   // SK하이닉스
    {"005930", 0},   // 삼성전자
    {"035420", 0},  // NAVER
    {"272210", 0}    // 한화시스템
};

void print_stock_list() {
    printf("최신 종목 정보:\n");
    printf("+------------+---------------+\n");
    printf("| Stock Code | Closing Price |\n");
    printf("+------------+---------------+\n");
    for (int i = 0; i < MAX_STOCKS; i++) {
        printf("| %-10s | %-13d |\n", stock_list[i].stock_code, stock_list[i].closing_price);
    }
    printf("+------------+---------------+\n\n");
}
// 전날 종가 조회
void get_latest_stock_info(MYSQL *conn) {
    char query[256];
    for (int i = 0; i < MAX_STOCKS; i++) {
        snprintf(query, sizeof(query),
             "SELECT stock_code, closing_price FROM market_prices "
             "WHERE stock_code = '%s' ORDER BY time DESC LIMIT 1;", stock_list[i].stock_code);
        if (mysql_query(conn, query)) {
            printf("MySQL query error: %s\n", mysql_error(conn));
            log_message("ERROR", "StockData", "전날 종가 데이터 확인 실패 (종목코드: %s)",stock_list[i].stock_code);
            return;
        }
        MYSQL_RES *result = mysql_store_result(conn);
        MYSQL_ROW row;

        // stock_list의 closing_price 업데이트
        if ((row = mysql_fetch_row(result))) {
            stock_list[i].closing_price = atoi(row[1]);  // 최신 종가 업데이트
        } else {
            printf("종목 %s의 최신 데이터 없음.\n", stock_list[i].stock_code);
        }

        mysql_free_result(result);
        log_message("NOTICE", "StockData", "전날 종가 데이터 확인 (종목코드: %s, 전날종가: %d)",stock_list[i].stock_code,stock_list[i].closing_price);
    }
    print_stock_list();
}
// 종목 코드 기반으로 closing_price 찾는 함수
int get_closing_price(const char *stock_code) {
    for (int i = 0; i < MAX_STOCKS; i++) {
        if (strcmp(stock_list[i].stock_code, stock_code) == 0) {
            return stock_list[i].closing_price;
        }
    }
    return 0;  // 종목 없음
}

void process_orders() {
    // 메시지 큐 생성 (초기화)
    int order_queue_id = init_message_queue(ORDER_QUEUE_ID);
    int stock_system_que_id = init_message_queue(STOCK_SYSTEM_QUEUE_ID);
    int execution_queue_id = init_message_queue(EXECUTION_QUEUE_ID);

    // 데이터베이스와 연결
    MYSQL *conn = connect_to_mysql();
    get_latest_stock_info(conn);

    fkq_order order;
    printf("매칭 엔진 대기 중...\n");
    log_message("NOTICE", "Server", "매칭 엔진 대기 중...");

    while (1) {
        // 주문 수신 프로세스의 주문 정보 전달을 기다림
        ssize_t bytes_received = msgrcv(order_queue_id, &order, sizeof(fkq_order), 0, 0);

        // 주문 정보 수신
        printf("주문 수신: 종목 %s, 가격 %d, 수량 %d\n", order.stock_code, order.price, order.quantity);
        log_message("INFO", "OrderProcessor", "주문 수신 - 종목: %s, 거래코드: %s, 유저: %s, 수량: %d, 가격: %d",
                order.stock_code, order.transaction_code, order.user_id, order.quantity, order.price);

        // 주문 데이터 DB에 기록
        insert_order(conn, &order);

        // 오류인 경우 파악해 추가 (구매 가능 수량 이상을 요청한 경우, 상한가 초과 하한가 미만의 호가 요청)

        // 상한가, 하한가 범위 외의 주문 요청
        int closing_price = get_closing_price(order.stock_code);
        int upper_limit = closing_price * 1.3;
        int lower_limit = closing_price * 0.7;

        if (order.price > upper_limit || order.price < lower_limit) {
            printf("주문 가격 초과: %d (허용 범위: %d ~ %d)\n", order.price, lower_limit, upper_limit);
            log_message("WARNING", "OrderProcessor", "상한가, 하한가를 벗어난 주문 요청 - 종목: %s, 주문 가격: %d (허용 범위: %d ~ %d)", order.stock_code, order.price, lower_limit, upper_limit);

            // 체결 오류 반환 (거래 불가)
            kft_execution execution;
            execution.header.tr_id = KFT_EXECUTION;
            execution.header.length = sizeof(kft_execution);
            strcpy(execution.transaction_code, order.transaction_code);
            execution.status_code = 99;  // 오류 코드
            get_timestamp(execution.time);
            execution.executed_price = 0; // 체결 가격 없음
            strcpy(execution.original_order, order.original_order);
            strcpy(execution.reject_code, "E003"); // 가격 초과 오류 코드
            
            send_execution_to_queue(execution_queue_id, &execution);
            continue;
        }


        // 체결 여부 결정 (거래 코드 뒷자리가 3 또는 7이면 미체결)
        if (order.transaction_code[5] == '3' || order.transaction_code[5] == '7') {
            printf("미체결 처리. 거래 코드: %s\n", order.transaction_code);
            log_message("INFO", "OrderProcessor", "미체결 주문. 거래 코드: %s", order.transaction_code);
            
            // 시세 프로세스에 미체결된 주문 전달
            send_to_queue(stock_system_que_id, 2, order.stock_code, order.order_type,  order.price, order.quantity);
            
            continue;
        }

        // 체결 된 경우, 체결 전문 작성
        kft_execution execution;
    
        execution.header.tr_id=KFT_EXECUTION;
        execution.header.length = sizeof(kft_execution);
        strcpy(execution.transaction_code, order.transaction_code);
        execution.status_code = 0;
        get_timestamp(execution.time);
        execution.executed_price = order.price;
        strcpy(execution.original_order, order.original_order);
        strcpy(execution.reject_code, "0000");
        
        // 체결 데이터 DB에 기록
        insert_execution(conn, &execution, order.quantity);
        log_message("INFO", "OrderProcessor", "거래 체결. 거래 코드: %s", order.transaction_code);
            
        // 시세 프로세스에 체결된 주문 정보 전달
        send_to_queue(stock_system_que_id, 1, order.stock_code, order.order_type,  execution.executed_price,order.quantity);

        // 체결 전문 송신 프로세스에 체결 전문 전달
        send_execution_to_queue(execution_queue_id,&execution);
        

    }
}


int main() {
    printf("매칭 엔진 시작\n");
    log_message("NOTICE", "Server", "매칭 엔진 서버 시작.");
    process_orders();
    return 0;
}