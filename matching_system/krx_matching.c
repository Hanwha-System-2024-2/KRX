#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include "../headers/kft_ipc.h"
#include "../headers/kmt_common.h"

#define MATCH_LOG_FILE "../log/match_server.log"
#define BUFFER_SIZE 512
#define MAX_TRANSACTIONS 100  // 최대 거래 코드 저장 개수


// match_server.log에서 ExecutionMessage 구조체를 추출하는 함수
int extract_execution_messages(const char *filename, ExecutionMessage messages[MAX_TRANSACTIONS], int *count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    *count = 0;

    while (fgets(buffer, BUFFER_SIZE, file)) {
        if (strncmp(buffer, "[INFO]", 6) == 0) {
            if (sscanf(buffer, "[INFO] Stock Code: %6s, Transaction Code: %6s, Order Type: %c, Execution: %d, Price: %d, Quantity: %d",
                messages[*count].stock_code,
                messages[*count].transaction_code,
                &messages[*count].order_type,
                &messages[*count].exectype,
                &messages[*count].price,
                &messages[*count].quantity) == 6) {
                (*count)++;
                if (*count >= MAX_TRANSACTIONS) break;  // 최대 저장 개수 초과 방지
            }
        }
    }

    fclose(file);
    return 0;
}

// update_market.log에서 Transaction Code만 추출하는 함수
int extract_transaction_codes(const char *filename, char transaction_codes[MAX_TRANSACTIONS][7], int *count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening update_market.log");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    *count = 0;


    while (fgets(buffer, BUFFER_SIZE, file)) {
        // printf("buffer: %s\n", buffer);
        if (strncmp(buffer, "[INFO]", 6) == 0) {
            char transaction_code[7];
            
            if (sscanf(buffer, "[INFO] Stock Code: %*[^,], Transaction Code: %6s", transaction_code) == 1) {
                strcpy(transaction_codes[*count], transaction_code);
                // printf("transaction code: %s", transaction_code);
                (*count)++;
                if (*count >= MAX_TRANSACTIONS) break;  // 최대 저장 개수 초과 방지
            }
        }
    }

    fclose(file);

    // 디버깅용 출력 (정상적으로 추출되었는지 확인)
    printf("Extracted transaction codes from update_market.log: ");
    for (int i = 0; i < *count; i++) {
        printf("%s ", transaction_codes[i]);
    }
    printf("\n");

    return 0;
}

// 메시지 큐를 통해 데이터 전송하는 함수
void send_message_queue(const ExecutionMessage *msg) {
    int msgid = msgget(STOCK_SYSTEM_QUEUE_ID, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget failed");
        return;
    }

    if (msgsnd(msgid, msg, sizeof(ExecutionMessage) - sizeof(long), 0) == -1) {
        printf("msgsnd failed\n");
    } else {
        printf("Message sent: Transaction Code %s missing in update_market.log\n", msg->transaction_code);
    }
}

// 지속적으로 로그를 확인하는 함수
void monitor_logs() {
    ExecutionMessage match_messages[MAX_TRANSACTIONS];
    char update_transactions[MAX_TRANSACTIONS][7];
    int match_count = 0, update_count = 0;

    extract_execution_messages(MATCH_LOG_FILE, match_messages, &match_count);
    extract_transaction_codes(UPDATE_MARKET_LOG_FILE, update_transactions, &update_count);

    printf("Match log transactions: ");
    for (int i = 0; i < match_count; i++) {
        printf("%s ", match_messages[i].transaction_code);
    }
    printf("\n");

    printf("Update market log transactions: ");
    for (int i = 0; i < update_count; i++) {
        printf("%s ", update_transactions[i]);
    }
    printf("\n");

    // `match_server.log`의 모든 Transaction Code가 `update_market.log`에 존재하는지 확인
    for (int i = 0; i < match_count; i++) {
        int found = 0;
        for (int j = 0; j < update_count; j++) {
            if (strcmp(match_messages[i].transaction_code, update_transactions[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("Missing transaction code detected: %s\n", match_messages[i].transaction_code);
            // send_message_queue(&match_messages[i]);  // 메시지 큐 전송
        }
    }
}

int main() {
    printf("Starting transaction log monitoring...\n");
    monitor_logs();
    return 0;
}
