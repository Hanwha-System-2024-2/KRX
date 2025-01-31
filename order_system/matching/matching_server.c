#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../../headers/kft_log.h"
#include "../../headers/kft_ipc_db.h"

 
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX_STOCKS 4

typedef struct {
    char stock_code[7];  // 종목 코드 (6자리 + NULL)
    int closing_price;   // 전날 종가
    int buying_balance;  // 매수 잔량
    int selling_balance; // 매도 잔량
    int bid_price;       // 매수 호가 
    int ask_price;       // 매도 호가 
} tmp_stock_info;


// 종목 정보 초기화
tmp_stock_info stock_list[MAX_STOCKS] = {
    {"000660", 0, 0, 0, 0, 0},   // SK하이닉스
    {"005930", 0, 0, 0, 0, 0},   // 삼성전자
    {"035420", 0, 0, 0, 0, 0},   // NAVER
    {"272210", 0, 0, 0, 0, 0}    // 한화시스템
};

void print_stock_list() {
    printf("\n최신 종목 정보:\n");
    printf("+------------+---------------+---------------+---------------+-----------+-----------+\n");
    printf("| Stock Code | Closing Price | BuyingBalance | SellingBalance | Bid Price | Ask Price |\n");
    printf("+------------+---------------+---------------+---------------+-----------+-----------+\n");
    for (int i = 0; i < MAX_STOCKS; i++) {
        printf("| %-10s | %-13d | %-13d | %-13d | %-9d | %-9d |\n",
               stock_list[i].stock_code, stock_list[i].closing_price,
               stock_list[i].buying_balance, stock_list[i].selling_balance,
               stock_list[i].bid_price, stock_list[i].ask_price);
    }
    printf("+------------+---------------+---------------+---------------+-----------+-----------+\n\n");
}

// 종목별 시세 정보 조회
void get_latest_stock_info(MYSQL *conn) {
    char query[256];
    for (int i = 0; i < MAX_STOCKS; i++) {

        snprintf(query, sizeof(query),
            "SELECT stock_code, closing_price, buying_balance, selling_balance, bid_price, ask_price "
            "FROM market_prices "
            "WHERE stock_code = '%s' ORDER BY time DESC LIMIT 1;", stock_list[i].stock_code);

        if (mysql_query(conn, query)) {
            printf("MySQL query error: %s\n", mysql_error(conn));
            log_message("ERROR", "StockData", "시세 데이터 확인 실패 (종목코드: %s)",stock_list[i].stock_code);
            return;
        }

        MYSQL_RES *result = mysql_store_result(conn);
        MYSQL_ROW row;

        // stock_list의 데이터 업데이트
        if ((row = mysql_fetch_row(result))) {
            stock_list[i].closing_price = atoi(row[1]);    // 전날 종가
            stock_list[i].buying_balance = atoi(row[2]);   // 매수 잔량
            stock_list[i].selling_balance = atoi(row[3]);  // 매도 잔량
            stock_list[i].bid_price = atoi(row[4]);        // 매수 호가
            stock_list[i].ask_price = atoi(row[5]);        // 매도 호가
        } else {
            printf("종목 %s의 최신 데이터 없음.\n", stock_list[i].stock_code);
        }

        mysql_free_result(result);
        log_message("NOTICE", "StockData", "종목 데이터 확인 (종목코드: %s, 전날종가: %d, 매수잔량: %d, 매도잔량: %d, 매수호가: %d, 매도호가: %d)",
                    stock_list[i].stock_code, stock_list[i].closing_price, stock_list[i].buying_balance,
                    stock_list[i].selling_balance, stock_list[i].bid_price, stock_list[i].ask_price);
    }
    print_stock_list();
}

// 시세 정보 업데이트 함수
void update_stock_info(ResultStockMessage *msg) {
    for (int i = 0; i < MAX_STOCKS; i++) {
        if (strcmp(stock_list[i].stock_code, msg->stock_code) == 0) {
            if (msg->quantity.trading_type == 0) {
                stock_list[i].buying_balance = msg->quantity.balance;
                stock_list[i].bid_price = msg->quantity.price;
                printf("%s - 매수 호가: %d원, 잔량: %d\n", stock_list[i].stock_code, stock_list[i].bid_price, stock_list[i].buying_balance);
            } else if (msg->quantity.trading_type == 1) {
                stock_list[i].selling_balance = msg->quantity.balance;
                stock_list[i].ask_price = msg->quantity.price;
                printf("%s - 매도 호가: %d원, 잔량: %d\n", stock_list[i].stock_code, stock_list[i].ask_price, stock_list[i].selling_balance);
            }
            return;
        }
    }
}

void process_orders() {
    // 메시지 큐 생성 (초기화)
    int order_queue_id = init_message_queue(ORDER_QUEUE_ID);
    int stock_system_que_id = init_message_queue(STOCK_SYSTEM_QUEUE_ID);
    int execution_queue_id = init_message_queue(EXECUTION_QUEUE_ID);
    int execution_result_stock_queue_id = init_message_queue(EXECUTION_RESULT_STOCK_QUEUE_ID);

    // 데이터베이스와 연결
    MYSQL *conn = connect_to_mysql();
    get_latest_stock_info(conn);

    fkq_order order;
    printf("매칭 엔진 대기 중...\n");
    log_message("NOTICE", "Server", "매칭 엔진 대기 중...");

    while (1) {

        // 주문 수신 프로세스의 주문 정보 전달을 기다림
        ssize_t bytes_received = msgrcv(order_queue_id, &order, sizeof(fkq_order), 0, 0);
        // 시세 수신 메시지 큐 다 empty할 때 까지 받아서 업데이트 해줘
        ResultStockMessage rstmsg;
    
        while (msgrcv(execution_result_stock_queue_id, &rstmsg, sizeof(ResultStockMessage), 0, IPC_NOWAIT) != -1) {
            printf("sdsdsdsdsdsdsds %d\n", rstmsg.quantity.balance);
            update_stock_info(&rstmsg);
        }
        
        if (errno == ENOMSG) {
            printf("시세 데이터 최신화 완료. 큐가 비었습니다.\n");
        } else {
            perror("msgrcv error");
        }

        // 주문 정보 수신
        printf("주문 수신: 종목 %s, 가격 %d, 수량 %d\n", order.stock_code, order.price, order.quantity);
        log_message("INFO", "OrderProcessor", "주문 수신 - 종목: %s, 거래코드: %s, 유저: %s, 수량: %d, 가격: %d",
                order.stock_code, order.transaction_code, order.user_id, order.quantity, order.price);

        // 주문 데이터 DB에 기록
        insert_order(conn, &order);

        // 오류인 경우 파악해 추가 (구매 가능 수량 이상을 요청한 경우, 상한가 초과 하한가 미만의 호가 요청)
        kft_execution execution;

        int stock_index = -1;
        for (int i = 0; i < MAX_STOCKS; i++) {
            if (strcmp(stock_list[i].stock_code, order.stock_code) == 0) {
                stock_index = i;
                break;
            }
        }
        // 종목이 없으면 주문 거부
        if (stock_index == -1) {
            printf("유효하지 않은 종목 코드: %s\n", order.stock_code);
            execution.header.tr_id = KFT_EXECUTION;
            execution.header.length = sizeof(kft_execution);
            strcpy(execution.transaction_code, order.transaction_code);
            execution.status_code = 99;  // 오류 코드
            get_timestamp(execution.time);
            execution.executed_price = 0; // 체결 가격 없음
            strcpy(execution.original_order, order.original_order);
            strcpy(execution.reject_code, "E206");
            send_execution_to_queue(execution_queue_id, &execution);
            continue;
        }

        // 상한가, 하한가 범위 외의 주문 요청
        int closing_price = stock_list[stock_index].closing_price;
        int upper_limit = closing_price * 1.3;
        int lower_limit = closing_price * 0.7;

        if (order.price > upper_limit || order.price < lower_limit) {
            printf("주문 가격 초과: %d (허용 범위: %d ~ %d)\n", order.price, lower_limit, upper_limit);
            log_message("WARNING", "OrderProcessor", "상한가, 하한가를 벗어난 주문 요청 - 종목: %s, 주문 가격: %d (허용 범위: %d ~ %d)", order.stock_code, order.price, lower_limit, upper_limit);

            // 체결 오류 반환 (거래 불가)
            
            execution.header.tr_id = KFT_EXECUTION;
            execution.header.length = sizeof(kft_execution);
            strcpy(execution.transaction_code, order.transaction_code);
            execution.status_code = 99;  // 오류 코드
            get_timestamp(execution.time);
            execution.executed_price = 0; // 체결 가격 없음
            strcpy(execution.original_order, order.original_order);
            strcpy(execution.reject_code, "E203"); // 가격 초과 오류 코드
            
            send_execution_to_queue(execution_queue_id, &execution);
            continue;
        }


        // 매수/매도 잔량 부족 여부 확인
        int available_buying_balance = stock_list[stock_index].buying_balance;
        int available_selling_balance = stock_list[stock_index].selling_balance;
        
        if (order.order_type == 'S' && order.quantity > available_selling_balance) {
            printf("주문 거부: 매도 잔량 부족 (주문 수량: %d, 가용 매도 잔량: %d)\n",
                   order.quantity, available_selling_balance);
            log_message("WARNING", "OrderProcessor", "주문 거부 - 매도 잔량 부족: 종목: %s, 주문 수량: %d, 가용 매도 잔량: %d",
                        order.stock_code, order.quantity, available_selling_balance);
            
            // 체결 오류 반환 (거래 불가)
            execution.header.tr_id = KFT_EXECUTION;
            execution.header.length = sizeof(kft_execution);
            strcpy(execution.transaction_code, order.transaction_code);
            execution.status_code = 99;  // 오류 코드
            get_timestamp(execution.time);
            execution.executed_price = 0; // 체결 가격 없음
            strcpy(execution.original_order, order.original_order);
            strcpy(execution.reject_code, "E204");
            send_execution_to_queue(execution_queue_id, &execution);

            continue;
        }

        if (order.order_type == 'B' && order.quantity > available_buying_balance) {
            printf("주문 거부: 매수 잔량 부족 (주문 수량: %d, 가용 매수 잔량: %d)\n",
                   order.quantity, available_buying_balance);
            log_message("WARNING", "OrderProcessor", "주문 거부 - 매수 잔량 부족: 종목: %s, 주문 수량: %d, 가용 매수 잔량: %d",
                        order.stock_code, order.quantity, available_buying_balance);

            execution.header.tr_id = KFT_EXECUTION;
            execution.header.length = sizeof(kft_execution);
            strcpy(execution.transaction_code, order.transaction_code);
            execution.status_code = 99;  // 오류 코드
            get_timestamp(execution.time);
            execution.executed_price = 0; // 체결 가격 없음
            strcpy(execution.original_order, order.original_order);
            strcpy(execution.reject_code, "E205");
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