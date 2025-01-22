#include "../headers/kft_ipc.h"

int message_queue_id = -1; // 메시지 큐 ID (초기값: -1)

// 메시지 큐 초기화 (한 번만 호출됨)
void init_message_queue() {
    message_queue_id = msgget((key_t)1234, IPC_CREAT | 0666);
    if (message_queue_id == -1) {
        perror("메시지 큐 생성 실패");
        log_message("ERROR", "Server", "메시지 큐 생성 실패");
        exit(1);
    }
    printf("메시지 큐 생성 완료 (ID: %d)\n", message_queue_id);
    log_message("NOTICE", "Server", "메시지 큐 생성 완료 (ID: %d)\n", message_queue_id);
}

// 메시지 큐에 데이터 전송
void send_to_queue(long msgtype, char *stock_code, char order_type, int price, int quantity) {
    if (message_queue_id == -1) {
        printf("메시지 큐가 초기화되지 않았습니다.\n");
        return;
    }

    ExecutionMessage msg;
    msg.msgtype = msgtype;
    strcpy(msg.stock_code, stock_code);
    msg.order_type = order_type;
    msg.price = price;
    msg.quantity = quantity;
    
    if (msgsnd(message_queue_id, &msg, sizeof(msg), IPC_NOWAIT) == -1) {
        perror("msgsnd() 실패");
        log_message("ERROR", "MessageQueue", "msgsnd() 실패");
    } else {
        printf("msgsnd() 성공: %s, 수량 %d, 가격 %d\n", stock_code, quantity, price);
        log_message("NOTICE", "MessageQueue","msgsnd() 성공: %s, 수량 %d, 가격 %d\n", stock_code, quantity, price);
    }
}