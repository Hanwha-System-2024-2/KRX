#ifndef KMT_MESSAGES_H
#define KMT_MESSAGES_H

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
  char stock_code[7];       // 종목코드
  char stock_name[51];      // 종목명
  int price;              // 시세(현재가)
  long volume;              // 거래량
  int change;               // 대비
  char rate_of_change[11];  // 등락률
  hoga hoga[2];             // 호가
  int high_price;           // 고가
  int low_price;            // 저가
  char market_time[19];     // 시세 형성 시간
} kmt_current_market_price;

typedef struct {
	hdr header;
	kmt_current_market_price body[4];
} kmt_current_market_prices;


// 종목 정보 구조체
typedef struct {
	char stock_code[7];
	char stock_name[51];
} stock_info;

// 종목 정보 송수신 구조체
typedef struct {
	hdr header;
	stock_info body[4]; // 총 종목이 4개로 구성
} kmt_stock_infos;

#endif
