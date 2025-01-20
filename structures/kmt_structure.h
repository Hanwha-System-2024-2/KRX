typedef struct { // 헤더
	char tr_id[4];
	int length;
} hdr;

typedef struct { // 호가
    int trading_type;
    int price;
    int balance;
}hoga;

typedef struct { //시세정보
  char stock_code[6];       // 종목코드
  char stock_name[50];      // 종목명
  float price;              // 시세(현재가)
  long volume;              // 거래량
  int change;               // 대비
  char rate_of_change[10];  // 등락률
  hoga hoga[2];             // 호가
  int high_price;           // 고가
  int low_price;            // 저가
  char market_time[18];     // 시세 형성 시간
} kmt_current_market_price;

typedef struct {
	hdr header;
	kmt_current_market_price body[4];
} kmt_current_markets_prices;









