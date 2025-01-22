#include <mysql/mysql.h>
#include "kft_messages.h"

// MySQL 연결 함수
MYSQL *connect_to_mysql();

// 주문 데이터 저장
void insert_order(MYSQL *conn, fkq_order *order);

// 체결 데이터 저장
void insert_execution(MYSQL *conn, kft_execution *execution, const int executed_quantity);
