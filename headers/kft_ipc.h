#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "../headers/krx_messages.h"
#include <mysql/mysql.h>

#define ORDER_QUEUE_ID 0001
#define STOCK_SYSTEM_QUEUE_ID 1234
#define EXECUTION_QUEUE_ID 5678
#define EXECUTION_RESULT_STOCK_QUEUE_ID 0102

typedef struct  {
    long msgtype;  // 체결1 미체결2
    char stock_code[7];  // 종목 코드
    char order_type;     // 'B' (매수) or 'S' (매도)
    int price;  // 체결 가격
    int quantity; // 체결 수량
} ExecutionMessage ;

typedef struct  {
    char stock_code[7];  // 종목 코드
    hoga quantity[2]; 
} ResultStockMessage ;
  

extern int message_queue_id;

// 메시지 큐 초기화
int init_message_queue(int key_id);

// 메시지 큐로 데이터 전송
void send_to_queue(int message_queue_id, long msgtype, char *stock_code, char order_type, int price, int quantity);
void send_order_to_queue(int message_queue_id, fkq_order *msg);
void send_execution_to_queue(int message_queue_id, kft_execution *msg);



// MySQL 연결 함수
MYSQL *connect_to_mysql();

// 주문 데이터 저장
void insert_order(MYSQL *conn, fkq_order *order);

// 체결 데이터 저장
void insert_execution(MYSQL *conn, kft_execution *execution, const int executed_quantity);
