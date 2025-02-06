#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../headers/kft_ipc.h"
#include "../headers/kmt_common.h"

#define MATCH_LOG_FILE "../log/match_server.log"
#define BUFFER_SIZE 512
#define MAX_TRANSACTIONS 100  // 최대 거래 코드 저장 개수


// 전송된 트랜잭션 코드 저장 (배열 + 정렬된 상태 유지)
static char sent_transactions[MAX_TRANSACTIONS][7];
static int sent_count = 0;
static long last_match_log_pos = 0;  // 마지막으로 읽은 위치 저장
static long last_update_log_pos = 0; // 마지막으로 읽은 위치 저장

// 로그 파일 크기 확인 함수 (파일 끝 위치 반환)
long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size; // 파일 크기 반환
    }
    return -1; // 오류 시
}

int binary_search_transaction(const char *transaction_code[7]) {
    int left = 0, right = sent_count - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        int cmp = strcmp(sent_transactions[mid], transaction_code);
        if (cmp == 0) return 1; // 이미 전송됨
        else if (cmp < 0) left = mid + 1;
        else right = mid - 1;
    }
    return 0; // 전송되지 않음
}


void insert_sorted_transaction(const char *transaction_code[7]) {
    int i;
    for (i = sent_count - 1; i >= 0 && strcmp(sent_transactions[i], transaction_code) > 0; i--) {
        strcpy(sent_transactions[i + 1], sent_transactions[i]); // 오른쪽으로 이동
    }
    strcpy(sent_transactions[i + 1], transaction_code);
    sent_count++;
}

// match_server.log에서 ExecutionMessage 구조체를 추출하는 함수
int extract_execution_messages(const char *filename, ExecutionMessage messages[MAX_TRANSACTIONS], int *count, long *last_pos) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    // 파일 크기 확인
    long file_size = get_file_size(filename);
    if (*last_pos >= file_size) {
        fclose(file);
        return 0; // 새로운 데이터 없음
    }

    fseek(file, *last_pos, SEEK_SET); // 마지막 위치부터 읽기
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
                    messages[*count].msgtype=1;
                    (*count)++;
                    if (*count >= MAX_TRANSACTIONS) break;
            }
        }
    }

    *last_pos = ftell(file); // 읽은 후 마지막 위치 저장
    fclose(file);
    return 0;
}

// update_market.log에서 Transaction Code만 추출하는 함수
int extract_transaction_codes(const char *filename, char transaction_codes[MAX_TRANSACTIONS][7], int *count, long *last_pos) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening update_market.log");
        return -1;
    }

    long file_size = get_file_size(filename);
    if (*last_pos >= file_size) {
        fclose(file);
        return 0; // 새로운 데이터 없음
    }

    fseek(file, *last_pos, SEEK_SET);
    char buffer[BUFFER_SIZE];
    *count = 0;

    while (fgets(buffer, BUFFER_SIZE, file)) {
        if (strncmp(buffer, "[INFO]", 6) == 0) {
            char transaction_code[7];
            if (sscanf(buffer, "[INFO] Stock Code: %*[^,], Transaction Code: %6s", transaction_code) == 1) {
                strcpy(transaction_codes[*count], transaction_code);
                (*count)++;
                if (*count >= MAX_TRANSACTIONS) break;
            }
        }
    }

    *last_pos = ftell(file); // 읽은 후 마지막 위치 저장
    fclose(file);
    return 0;
}

// 메시지 큐를 통해 데이터 전송하는 함수
void send_message_queue(const ExecutionMessage *msg) {
    if (binary_search_transaction(msg->transaction_code)) {
        printf("이미 전송된 Transaction Code: %s\n", msg->transaction_code);
        return;
    }

    int msgid = msgget(STOCK_SYSTEM_QUEUE_ID, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget failed");
        return;
    }

    if (msgsnd(msgid, msg, sizeof(ExecutionMessage) - sizeof(long), 0) == -1) {
        printf("msgsnd failed\n");
    } else {
        printf("Message sent: Transaction Code %s\n", msg->transaction_code);
        insert_sorted_transaction(msg->transaction_code);
    }
}

// 지속적으로 로그를 확인하는 함수
void monitor_logs() {
    while(1) {
        ExecutionMessage match_messages[MAX_TRANSACTIONS];
        char update_transactions[MAX_TRANSACTIONS][7];
        int match_count = 0, update_count = 0;
        
        extract_execution_messages(MATCH_LOG_FILE, match_messages, &match_count, &last_match_log_pos);
        extract_transaction_codes(UPDATE_MARKET_LOG_FILE, update_transactions, &update_count, &last_update_log_pos);

        printf("Match log transactions: ");
    
        // `match_server.log`의 모든 Transaction Code가 `update_market.log`에 존재하는지 확인
        for (int i = 0; i < match_count; i++) {
            int found = 0;
            
            for (int j = 0; j < update_count; j++) {
                if (strcmp(match_messages[i].transaction_code, update_transactions[j]) == 0) {
                    found = 1;
                    break;
                }
            }
            // 매칭이 안됐고, 재전송 목록에 미포함된 주문 코드이면 재전송
            if (!found && !binary_search_transaction(&match_messages[i].transaction_code)) {
                printf("Missing transaction code detected: %s\n", match_messages[i].transaction_code);
                send_message_queue(&match_messages[i]);  // 메시지 큐 전송
            }
        }
        sleep(5);
    }
    
}

int main() {
    printf("Starting transaction log monitoring...\n");
    monitor_logs();
    
    return 0;
}
