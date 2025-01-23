#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../headers/kft_log.h"
#include "../../headers/kft_ipc.h"
#include "../../headers/kft_db_sql.h"


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>


void process_orders() {
    int order_queue_id = init_message_queue(ORDER_QUEUE_ID);
    int stock_system_que_id = init_message_queue(STOCK_SYSTEM_QUEUE_ID);
    int execution_queue_id = init_message_queue(EXECUTION_QUEUE_ID);

    MYSQL *conn = connect_to_mysql();

    fkq_order order;
    printf("매칭 엔진 대기 중...\n");
    log_message("NOTICE", "Server", "매칭 엔진 대기 중...");

    while (1) {
        ssize_t bytes_received = msgrcv(order_queue_id, &order, sizeof(fkq_order), 0, 0);

        printf("주문 수신: 종목 %s, 가격 %d, 수량 %d\n", order.stock_code, order.price, order.quantity);
        log_message("INFO", "OrderProcessor", "주문 수신 - 종목: %s, 거래코드: %s, 유저: %s, 수량: %d, 가격: %d",
                order.stock_code, order.transaction_code, order.user_id, order.quantity, order.price);


        insert_order(conn, &order);

        // 오류인 경우 파악해 추가 (구매 가능 수량 이상을 요청한 경우, 상한가 초과 하한가 미만의 호가 요청)

        // 체결 여부 결정 (거래 코드 뒷자리가 3 또는 7이면 미체결)
        if (order.transaction_code[5] == '3' || order.transaction_code[5] == '7') {
            printf("미체결 처리. 거래 코드: %s\n", order.transaction_code);
            log_message("INFO", "OrderProcessor", "미체결 주문. 거래 코드: %s", order.transaction_code);
            
            // 시세 프로세스에 미체결된 주문 전달
            send_to_queue(stock_system_que_id, 2, order.stock_code, order.order_type,  order.price, order.quantity);
            
            continue;
        }

        kft_execution execution;
    
        execution.header.tr_id=KFT_EXECUTION;
        execution.header.length = sizeof(kft_execution);
        strcpy(execution.transaction_code, order.transaction_code);
        execution.status_code = 0;
        get_timestamp(execution.time);
        execution.executed_price = order.price;
        strcpy(execution.original_order, order.original_order);
        strcpy(execution.reject_code, "0000");
        
        insert_execution(conn, &execution, order.quantity);
        


        log_message("INFO", "OrderProcessor", "거래 체결. 거래 코드: %s", order.transaction_code);
            
        // 시세 프로세스에 체결된 주문 정보 전달
        send_to_queue(stock_system_que_id, 1, order.stock_code, order.order_type,  execution.executed_price,order.quantity);

        send_execution_to_queue(execution_queue_id,&execution);
        

    }
}


int main() {
    printf("매칭 엔진 시작\n");
    log_message("NOTICE", "Server", "매칭 엔진 서버 시작.");
    process_orders();
    return 0;
}