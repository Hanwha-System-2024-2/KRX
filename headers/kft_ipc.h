#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "../headers/kft_messages.h"

#define ORDER_QUEUE_ID 0001
#define STOCK_SYSTEM_QUEUE_ID 1234
#define EXECUTION_QUEUE_ID 5678

typedef struct  {
    long msgtype;  // 체결1 미체결2
    char stock_code[7];  // 종목 코드
    char order_type;     // 'B' (매수) or 'S' (매도)
    int price;  // 체결 가격
    int quantity; // 체결 수량
} ExecutionMessage ;
  

extern int message_queue_id;

// 메시지 큐 초기화
int init_message_queue(int key_id);

// 메시지 큐로 데이터 전송
void send_to_queue(int message_queue_id, long msgtype, char *stock_code, char order_type, int price, int quantity);
void send_order_to_queue(int message_queue_id, fkq_order *msg);
void send_execution_to_queue(int message_queue_id, kft_execution *msg);