#ifndef KRX_MESSAGES_H
#define KRX_MESSAGES_H

#define FKQ_ORDER 9 
#define KFT_ORDER 10
#define KFT_EXECUTION 11

typedef struct { // 헤더
	int tr_id;
	int length;
} hdr;

typedef struct { // 호가
    int trading_type;
    int price;
    int balance;
}hoga;

typedef struct { //시세정보
  char stock_code[7]; char padding1;       // 종목코드
  char stock_name[51]; char padding2;      // 종목명
  int price;              // 시세(현재가)
  long volume;              // 거래량
  int change;               // 대비
  char rate_of_change[11]; char padding3;  // 등락률
  hoga hoga[2];             // 호가
  int high_price;           // 고가
  int low_price;            // 저가
  char market_time[15]; char padding4;   // 시세 형성 시간
} kmt_current_market_price;

typedef struct {
	hdr header;
	kmt_current_market_price body[4];
} kmt_current_market_prices;


// 종목 정보 구조체
typedef struct {
	char stock_code[7]; char padding1;
	char stock_name[51]; char padding2;
} stock_info;

// 종목 정보 송수신 구조체
typedef struct {
	hdr header;
	stock_info body[4]; // 총 종목이 4개로 구성
} kmt_stock_infos;

// 주문 요청 전문
typedef struct {
    hdr header;
    char stock_code[7]; char padding1;
    char stock_name[51]; char padding2;
    char transaction_code[7]; char padding3;
    char user_id[21]; char padding4[3];
    char order_type; char padding5[3];
    int quantity;
    char order_time[15]; char padding6;
    int price;
    char original_order[7]; char padding7;
} fkq_order;

// 주문 응답 전문
typedef struct {
    hdr header;
    char transaction_code[7]; char padding1;
    char user_id[21]; char padding2[3];
    char time[15]; char padding3;
    char reject_code[7]; char padding4;
} kft_order;

// 체결 응답 전문
typedef struct {
    hdr header;
    char transaction_code[7]; char padding1;
    int status_code;
    char time[15]; char padding2;
    int executed_price;
    char original_order[7]; char padding3;
    char reject_code[7]; char padding4;
} kft_execution;

typedef struct  {
    long msgtype;  // 1: 우선순위 2: 일반 3: 후순위
    int exectype;  // 체결1 미체결2
    char stock_code[7]; // 종목 코드
    char transaction_code[7]; // 거래코드
    char order_type;   // 'B' (매수) or 'S' (매도)
    int price;  // 체결 가격
    int quantity; // 체결 수량
} ExecutionMessage ;

typedef struct  {
    long msgtype;
    char stock_code[7];  // 종목 코드
    hoga quantity; 
} ResultStockMessage ;


#endif
