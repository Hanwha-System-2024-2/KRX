#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "../headers/kmt_common.h"


int main() {
    int key_id;
    ExecutionMessage msg;



    // 메시지 큐 생성 또는 연결
    key_id = msgget((key_t)1234, IPC_CREAT | 0666);
    if (key_id == -1) {
        perror("Message Get Failed");
        exit(1);
    }

    printf("Message queue connected. Sending messages...\n");

    while (1) {
        // 메시지 초기화
        msg.msgtype = 2;  // 메시지 유형
        msg.exectype =2;
        strcpy(msg.transaction_code, "112312");
        printf("Enter stock code (6 characters): ");
        scanf("%6s", msg.stock_code);

        printf("Enter order type ('B' for buy, 'S' for sell): ");
        scanf(" %c", &msg.order_type);

        printf("Enter price: ");
        scanf("%d", &msg.price);

        printf("Enter quantity: ");
        scanf("%d", &msg.quantity);

        // 메시지 전송
        if (msgsnd(key_id, &msg, sizeof(msg)-sizeof(long), 0) == -1) {
            perror("Message Send Failed");
            exit(1);
        }

        printf("Message sent: stock_code=%s, order_type=%c, price=%d, quantity=%d\n",
               msg.stock_code, msg.order_type, msg.price, msg.quantity);

        printf("Send another message? (y/n): ");
        char choice;
        scanf(" %c", &choice);
        if (choice == 'n' || choice == 'N') {
            break;
        }
    }

    printf("Exiting message sender.\n");
    return 0;
}

