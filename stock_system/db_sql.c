#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "/usr/include/mysql/mysql.h"
#include "../headers/kmt_common.h"
#include "../headers/kmt_messages.h"

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS "Hanwha1!"
#define DB_NAME "hanwha_krx"

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
	int idx=0;	
	kmt_current_market_prices data;
	// 잠깐 헤더 임의로 부여
	strcpy(data.header.tr_id, "TR01");
	data.header.length=sizeof(data.body);

	// 바디에 시세 데이터 추가
	while ((row = mysql_fetch_row(result))) {
		strcpy(data.body[i].stock_code, row[0] ? row[0] : "NULL");
		data.body[i].closing_price =  row[1] ? row[1] : -1;
		data.body[i].stock_price = row[2] ? row[2] : -1;
		data.body[i].volume= row[3] ? row[3] : -1;	
		data.body[i].contrast =row[4] ? row[4] : -1;
		strcpy(data.body[i].rate_of_change, row[5] ? row[5] : "NULL");
		data.body[i].hoga[0].trading_type=0; // 매수
		data.body[i].hoga[0].balance=row[6] ? row[6] : -1;
		data.body[i].hoga[1].trading_type=1; // 매도
		data.body[i].hoga[1].balance=row[7] ? row[7] : -1;
		data.body[i].hoga[0].price = row[8] ? row[8] : -1;
		data.body[i].hoga[1].price = row[9] ? row[9] : -1;
		data.body[i].high_price = row[10] ? row[10] : -1;
		data.body[i].low_price = row[11] ? row[11] : -1;
		strcpy(data.body[i].stock_name, row[12] ? row[12] : "NULL");
		format_current_time(data.body[i].market_time);
		

//        	for (int i = 0; i < num_fields; i++) {
 //      		printf("%s ", row[i] ? row[i] : "NULL");
//        	}
	printf("\n");
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

	int num_fields = mysql_num_fields(result);

	while((row = mysql_fetch_row(result))) {
		for(int i=0;i<num_fields;i++) {
			printf("%s ", row[i] ? row[i] : "NULL");
		}
		printf("\n");
	}
	// free MYSQL_RES
	mysql_free_result(result);

}

int updateMarketPrice(MYSQL *conn) {
	int result=1;
	




	return result;
}

void finish_with_error(MYSQL *con) {
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    exit(1);
}





