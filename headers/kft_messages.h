#define FKQ_ORDER 9 
#define KFT_ORDER 10
#define KFT_EXECUTION 11


// 전문 헤더
typedef struct {
    int tr_id;
    int length;
} hdr;

// 주문 요청 전문
typedef struct {
    hdr header;
    char stock_code[7];
    char stock_name[51];
    char transaction_code[7];
    char user_id[21];
    char order_type;
    int quantity;
    char order_time[15];
    int price;
    char original_order[7];
} fkq_order;

// 주문 응답 전문
typedef struct {
    hdr header;
    char transaction_code[7];
    char user_id[21];
    char time[15];
    char reject_code[7];
} kft_order;

// 체결 응답 전문
typedef struct {
    hdr header;
    char transaction_code[7];
    int status_code;
    char time[15];
    int executed_price;
    char original_order[7];
    char reject_code[7];
} kft_execution;