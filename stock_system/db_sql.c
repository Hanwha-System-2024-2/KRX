#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<memory.h>
#include<syslog.h>
#include "/usr/include/mysql/mysql.h"
#include "../headers/kmt_common.h"
#include "../headers/kmt_messages.h"

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS "Hanwha1!"
#define DB_NAME "hanwha_krx"


typedef struct  {
    long msgtype; 
    char stock_code[7];  // 종목 코드
    char order_type;     // 'B' (매수) or 'S' (매도)
    int price;  // 체결 가격
    int quantity; // 체결 수량
} msgbuf;

void finish_with_error(MYSQL *con) {
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    exit(1);
}


void format_current_time(char *buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, 15, "%Y%m%d%H%M%S", t); // "yyyymmddhhmmss"
}


MYSQL *connect_to_mysql() {
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() 실패\n");
        exit(1);
    }

    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
        fprintf(stderr, "MySQL 연결 실패: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    printf("MySQL 연결 성공!\n");
    return conn;  // 연결 객체 반환
}

kmt_current_market_prices getMarketPrice(MYSQL *conn) {
	if(mysql_query(conn, "select * from market_prices m join stock_info s on m.stock_code=s.stock_code")){
		finish_with_error(conn);
	}

	MYSQL_RES *result = mysql_store_result(conn);

	if (result == NULL) {
        	finish_with_error(conn);
    	}
	
	
	kmt_current_market_prices data;
	// 헤더 부여 : 시세 ID 8
	data.header.tr_id=8;
	data.header.length=sizeof(data);

	// 바디에 시세 데이터 추가
	int i=0;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {

		// DB -> structure
		strncpy(data.body[i].stock_code, row[0] ? row[0] : "NULL", strlen(row[0]));	
		data.body[i].stock_code[strlen(row[0])]='\0';
		data.body[i].price = row[2] ? atoi(row[2]) : -1;
		data.body[i].volume= row[3] ? atoi(row[3]) : -1;	
		data.body[i].change =row[4] ? atoi(row[4]) : -1;
		strncpy(data.body[i].rate_of_change, row[5] ? row[5] : "NULL", strlen(row[5]));
		data.body[i].rate_of_change[strlen(row[5])]='\0';
		data.body[i].hoga[0].trading_type=0; // 매수
		data.body[i].hoga[0].balance=row[6] ? atoi(row[6]) : -1;
		data.body[i].hoga[1].trading_type=1; // 매도
		data.body[i].hoga[1].balance=row[7] ? atoi(row[7]) : -1;
		data.body[i].hoga[0].price = row[8] ? atoi(row[8]) : -1;
		data.body[i].hoga[1].price = row[9] ? atoi(row[9]) : -1;
		data.body[i].high_price = row[10] ? atoi(row[10]) : -1;
		data.body[i].low_price = row[11] ? atoi(row[11]) : -1;
		strncpy(data.body[i].stock_name, row[14] ? row[14] : "NULL", strlen(row[14]));
		data.body[i].stock_name[strlen(row[14])]='\0';
		format_current_time(data.body[i].market_time);
		i++;		
	}
	// 보낸 로그 찍기
	printf("[SEND DATA: %s] \n", data.body[0].market_time);

	// free MYSQL_RES
	mysql_free_result(result);
	return data;
}

// 종목 정보 조회
kmt_current_market_prices getStockInfo(MYSQL *conn) {
	if(mysql_query(conn, "select * from stock_info")) {
		finish_with_error(conn);
	}	
	
	MYSQL_RES *result = mysql_store_result(conn);

	if(result==NULL) {
		finish_with_error(conn);
	}
	// 헤더 부여 : 종목 정보 헤더 ID : 7
	kmt_stock_infos data;
	data.header.tr_id=7;
	data.header.length=sizeof(data);
	

	MYSQL_ROW row;
	int i=0;
	while((row = mysql_fetch_row(result))) {
		strcpy(data.body[i].stock_code, row[0]);
		strcpy(data.body[i].stock_name, row[1]);
	}
	// free MYSQL_RES
	mysql_free_result(result);

}

int updateMarketPrices(MYSQL *conn, msgbuf* msg, int type) { //type 1: 체결, type 2: 미체결
	int result=1;
	char query[512];
	int closing_price=0;
	int contrast=0;
	int lowest_price;
	int highest_price;
	char fluctuation_rate[11];
	char market_time[19];
	format_current_time(market_time);
	
	
	
	// 전일 종가, 고가, 저가 불러오기
	if(mysql_query(conn, "select stock_code, closing_price, high_price, low_price  from market_prices")) {
		finish_with_error(conn);
	}
	MYSQL_RES *sql_result = mysql_store_result(conn);
	if(sql_result==NULL) {
		finish_with_error(conn);
	}

	MYSQL_ROW row;
	// 종목 코드 없는 경우 예외처리
	int existFlag=1;
	while((row=mysql_fetch_row(sql_result))) {
		
		if(strcmp(row[0], msg->stock_code)==0) { // 업데이트된 종목 코드일 때 시세 업데이트하기 
			closing_price = atoi(row[1]);
			highest_price = atoi(row[2]);
			lowest_price = atoi(row[3]);
			existFlag=0;
		}
	}
	// db에 없는 종목 코드이면 예외 처리 해주기
	if(existFlag) {
		printf("[ERROR: NO STOCK CODE]\n");
		return 0;
	}

	// 고가, 저가 갱신
	if(msg->price > highest_price) {
		snprintf(query, sizeof(query), 
		"UPDATE market_prices"
		"SET high_price = %d"
		"WHERE stock_code = '%s'",
		highest_price, msg->stock_code);
	}

	else if(msg->price < lowest_price) {
		snprintf(query, sizeof(query),
		"UPDATE market_prices"
		"SET low_price = %d"
		"WHERE stock_code = '%s'",		
		lowest_price, msg->stock_code);
	}	
	// 쿼리 초기화
	memset(query, '\0', sizeof(query));
		
	// 대비: 현재가 - 전일종가
	contrast=msg->price-closing_price;

	// 등락률: ((현재가 - 전일종가) / 전일종가) *100
	float f = (float)((float)(msg->price - closing_price) / closing_price) *100;		
	sprintf(fluctuation_rate, "%.2f%%", f);

	// 주문가가 상한가 또는 하한가 범위 밖일 때 에러처리? or 매수호가 범위 밖일 때 에러처리
	if(f>30.0 || f<-30.0) {
		printf("[ERROR: WRONG PRICE RANGE]\n");
		return 0;
	}


	if(type==1) { // 체결 : 현잔량 - 호가잔량
		if(msg->order_type == 'B') {
			snprintf(query, sizeof(query), 
	    		"UPDATE market_prices "
	            	"SET stock_price = %d, volume = volume + %d, buying_balance = buying_balance - %d, fluctuation_rate = '%s', contrast = %d, time = %s "
	       	     	"WHERE stock_code = '%s' AND buying_balance >= %d",
		    	msg->price, msg->quantity, msg->quantity, fluctuation_rate, contrast, market_time, msg->stock_code, msg->quantity);
		}  else if(msg->order_type == 'S') {
			snprintf(query, sizeof(query),
            		"UPDATE market_prices "
	            	"SET stock_price = %d, volume = volume + %d, selling_balance = selling_balance - %d, fluctuation_rate= '%s', contrast = %d, time = %s "
        	    	"WHERE stock_code = '%s' AND selling_balance >= %d",
			msg->price, msg->quantity, msg->quantity, fluctuation_rate, contrast, market_time, msg->stock_code, msg->quantity);
		} else {
		    fprintf(stderr, "Invalid order_type: %c\n", msg->order_type);
		    return 0;	
		}
	
	}
	else { // 미체결 : 현잔량 + 호가잔량
		if(msg->order_type == 'B') {
			snprintf(query, sizeof(query), 
		    	"UPDATE market_prices "
       		     	"SET stock_price = %d, volume = volume + %d, buying_balance = buying_balance + %d, fluctuation_rate = '%s', contrast = %d,  time =%s "
	       	     	"WHERE stock_code = '%s'",
		    	msg->price, msg->quantity, msg->quantity, fluctuation_rate, contrast, market_time, msg->stock_code, msg->quantity);
		}  else if(msg->order_type == 'S') {
			snprintf(query, sizeof(query),
            		"UPDATE market_prices "
	            	"SET stock_price = %d, volume = volume + %d, selling_balance = selling_balance + %d, fluctuation_rate= '%s', contrast = %d, time=%s "
        	    	"WHERE stock_code = '%s'",
			msg->price, msg->quantity, msg->quantity, fluctuation_rate, contrast, market_time, msg->stock_code, msg->quantity);
		} else {
		    fprintf(stderr, "Invalid order_type: %c\n", msg->order_type);
		    return 0;	
		}

	}
	
	// 쿼리 실행
	if (mysql_query(conn, query)) {
        	fprintf(stderr, "MySQL Query Error: %s\n", mysql_error(conn));
        	return 0;
	}
	
	

	// free sql result 
	mysql_free_result(sql_result);


	// 업데이트 확인 로그찍기
	printf("[LOG] Stock Code: %s, Price: %d, Quantity: %d, Fluctuation Rate: %s, Time: %s\n", 
        msg->stock_code, msg->price, msg->quantity, fluctuation_rate, market_time);
	// syslog로 로그 남기기
	// syslog 초기화
    openlog("MarketPriceUpdater", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Stock Code: %s, Price: %d, Quantity: %d, Fluctuation Rate: %s, Time: %s",
           msg->stock_code, msg->price, msg->quantity, fluctuation_rate, market_time);
	closelog(); // syslog 종료
	

	return result;
}







