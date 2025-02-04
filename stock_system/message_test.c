#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#define STOCK_SYSTEM_QUEUE_ID 1234
#define EXECUTION_RESULT_STOCK_QUEUE_ID 0102

typedef struct {
    long msgtype;   // 메시지 타입 (1: 체결, 2: 미체결)
    char stock_code[7]; // 종목 코드
    char order_type; // 'B' (매수) or 'S' (매도)
    int price;       // 주문 가격
    int quantity;    // 주문 수량
} ExecutionMessage;

typedef struct {
    int trading_type; // 0: 매수, 1: 매도
    int balance;      // 주문 후 잔량
    int price;        // 거래된 가격
} hoga;

typedef struct {
    long msgtype;      // 메시지 타입
    char stock_code[7];  // 종목 코드
    hoga quantity;       // 호가 정보 (잔량, 가격)
} ResultStockMessage;

void send_message() {
    key_t key = STOCK_SYSTEM_QUEUE_ID;
    int msgq_id = msgget(key, IPC_CREAT | 0666);
    if (msgq_id == -1) {
        perror("msgget() failed (send)");
        exit(1);
    }

    ExecutionMessage msg;
    msg.msgtype = 1; // 체결 메시지 타입
    strcpy(msg.stock_code, "005930");
    msg.order_type = 'B'; // 매수
    msg.price = 65000;
    msg.quantity = 10;

    if (msgsnd(msgq_id, &msg, sizeof(ExecutionMessage) - sizeof(long), IPC_NOWAIT) == -1) {
        perror("msgsnd failed");
        exit(1);
    }

    printf("[Sent] Stock: %s, Order: %c, Price: %d, Quantity: %d\n",
           msg.stock_code, msg.order_type, msg.price, msg.quantity);
}

void receive_message() {
    key_t key = EXECUTION_RESULT_STOCK_QUEUE_ID;
    int msgq_id = msgget(key, IPC_CREAT | 0666);
    if (msgq_id == -1) {
        perror("msgget() failed (receive)");
        exit(1);
    }

    ResultStockMessage received_msg;
    if (msgrcv(msgq_id, &received_msg, sizeof(ResultStockMessage) - sizeof(long), 0, IPC_NOWAIT) == -1) {
        perror("msgrcv failed");
        return; // 오류 발생해도 종료하지 않고 계속 실행
    }

    printf("[Received] Stock: %s, Balance: %d\n",
           received_msg.stock_code,
           received_msg.quantity.balance);
}

int main() {
    while (1) {
        // send_message();
        // sleep(2); // 1초 대기
        receive_message();
        sleep(2);
    }
    return 0;
}
