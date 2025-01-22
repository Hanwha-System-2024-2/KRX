#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<memory.h>
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

	int num_fields = mysql_num_fields(result);
	kmt_current_market_prices data;
	// 헤더 부여 : 시세 ID 8
	data.header.tr_id=8;
	data.header.length=sizeof(data.body);

	// 바디에 시세 데이터 추가
	int i=0;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		// init char* to '\0'
		//memset(data.body[i].stock_code, '\0', sizeof(data.body[i].stock_code));
		//memset(data.body[i].rate_of_change, '\0', sizeof(data.body[i].rate_of_change));
		//memset(data.body[i].stock_name, '\0', sizeof(data.body[i].stock_name));

		// DB -> structure
		strncpy(data.body[i].stock_code, row[0] ? row[0] : "NULL", strlen(row[0]));
		//printf("wwwwwwwww %d\n", strlen(row[0]));
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
		printf("stock name: %s\n",row[14]);
		printf("stock code: %s\n", data.body[i].stock_code); 
		format_current_time(data.body[i].market_time);
		i++;		
	}

	
	// free MYSQL_RES
	mysql_free_result(result);
	return data;
}

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
	data.header.length=sizeof(data.body);
	

	MYSQL_ROW row;
	int i=0;
	while((row = mysql_fetch_row(result))) {
		strcpy(data.body[i].stock_code, row[0]);
		strcpy(data.body[i].stock_name, row[1]);
	}
	// free MYSQL_RES
	mysql_free_result(result);

}

int updateMarketPrices(MYSQL *conn, msgbuf* msg) {
	int result=1;
	char query[512];

	if(msg->order_type == 'B') {
		snprintf(query, sizeof(query), 
	    	"UPDATE market_prices "
            	"SET stock_price = %d, buying_balance = buying_balance - %d "
            	"WHERE stock_code = '%s' AND buying_balance >= %d",
	    	msg->price, msg->quantity, msg->stock_code, msg->quantity);
	}  else if(msg->order_type == 'S') {
		snprintf(query, sizeof(query),
            	"UPDATE market_prices "
            	"SET stock_price = %d, selling_balance = selling_balance - %d "
            	"WHERE stock_code = '%s' AND selling_balance >= %d",
		msg->price, msg->quantity, msg->stock_code, msg->quantity);
	} else {
	    fprintf(stderr, "Invalid order_type: %c\n", msg->order_type);
	    return 0;	
	}

	// 쿼리 실행
	if (mysql_query(conn, query)) {
        	fprintf(stderr, "MySQL Query Error: %s\n", mysql_error(conn));
        	return 0;
	}


	// 업데이트 확인
	// 로그찍기
	
	

	return result;
}







