#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<memory.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "/usr/include/mysql/mysql.h"
#include "../headers/kmt_common.h"
#include "../headers/krx_messages.h"
#include "../headers/kft_ipc.h"

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS "Hanwha1!"
#define DB_NAME "hanwha_krx"


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

void getBalance(MYSQL* conn, char order_type, ResultStockMessageSnd* data) {
	char query[512];
	int selling_balance, buying_balance; // 매도잔량, 매수잔량
	int bid_price, ask_price; // 매도호가, 매수호가
	
	

	if(order_type=='B') { // 매수
		data->quantity.trading_type=0;
		snprintf(query, sizeof(query), 
			"SELECT buying_balance, ask_price "
			"FROM market_prices "
			"WHERE stock_code = %s",
			data->stock_code);
		

	} else { // 매도
		data->quantity.trading_type=1;
		snprintf(query, sizeof(query), 
			"SELECT selling_balance, bid_price "
			"FROM market_prices "
			"WHERE stock_code = %s",
			data->stock_code);
	}

	if(mysql_query(conn, query)) {
			finish_with_error(conn);
		}
		MYSQL_RES *sql_result = mysql_store_result(conn);
		if(sql_result==NULL) {
			finish_with_error(conn);
		}
		MYSQL_ROW row;
		
		while((row=mysql_fetch_row(sql_result))) {
			data->quantity.balance=atoi(row[0]);		
			data->quantity.price=atoi(row[1]);
		}

	
}

void sendResultMessage(ResultStockMessageSnd *data) {
	key_t key = EXECUTION_RESULT_STOCK_QUEUE_ID; // queue key value
	int msgq_id;
	data->msgtype=1;

	if((msgq_id = msgget(key, IPC_CREAT | 0666)) == -1)
	{
		perror("msgget() failed!");
		exit(1);
	}	
	

	if (msgsnd(msgq_id, data, sizeof(ResultStockMessageSnd)- sizeof(long), IPC_NOWAIT) == -1) {
		// 파일에 직접 기록
		FILE *log_file = fopen("/home/ec2-user/KRX/log/update_market_price.log", "a");
		if (log_file) {
			fprintf(log_file, "[Error Log] Stock Code: %s, Quantity: %d\n",
					data->stock_code, data->quantity.balance);
			fclose(log_file);
		}
        printf("msgsnd failed");
        return;
    }
	printf("[주문 전송 완료] 종목 코드: %s, 잔량: %d, 호가: %d\n", data->stock_code, data->quantity.balance, data->quantity.price);
    


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

int getClosingPriceByStockCode(MYSQL* conn, char* stock_code) {
	int closing_price=0;
	char query[512];
	// 전일 종가 불러오기
	snprintf(query, sizeof(query), 
		"select closing_price from market_prices "
		"where stock_code= %s",
		stock_code
	);
	if(mysql_query(conn, query)) {
		finish_with_error(conn);
	}
	MYSQL_RES *sql_result = mysql_store_result(conn);
	if(sql_result==NULL) {
		finish_with_error(conn);
	}

	MYSQL_ROW row;
	while((row=mysql_fetch_row(sql_result))) {		
		closing_price = atoi(row[0]);
			
	}
	return closing_price;
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
	memset(&data, 0, sizeof(data));
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
	// printf("[SEND DATA: %s] \n", data.body[0].market_time);

	// free MYSQL_RES
	mysql_free_result(result);
	return data;
}

// 종목 정보 조회
kmt_stock_infos getStockInfo(MYSQL *conn) {
	if(mysql_query(conn, "select * from stock_info")) {
		finish_with_error(conn);
	}	
	
	MYSQL_RES *result = mysql_store_result(conn);

	if(result==NULL) {
		finish_with_error(conn);
	}
	// 헤더 부여 : 종목 정보 헤더 ID : 14
	kmt_stock_infos data;
	data.header.tr_id=14;
	data.header.length=sizeof(data);
	

	MYSQL_ROW row;
	int i=0;
	while((row = mysql_fetch_row(result))) {
		strcpy(data.body[i].stock_code, row[0]);
		strcpy(data.body[i].stock_name, row[1]);
		i++;
	}
	// free MYSQL_RES
	mysql_free_result(result);
	return data;
}


void updateMarketPricesAuto(MYSQL* conn) {
	ExecutionMessage hanwha;
	ExecutionMessage samsung;
	hanwha.msgtype=1;
	samsung.msgtype=1;
	strcpy(hanwha.stock_code, "272210");
	strcpy(samsung.stock_code, "005930");

	// 매수/매도
	if(rand()%2==1) {
		hanwha.order_type='B';
		samsung.order_type='B';
	} else {
		hanwha.order_type='S';
		samsung.order_type='S';
	}

	// 가격 결정
	hanwha.price=getClosingPriceByStockCode(conn, hanwha.stock_code);
	samsung.price=getClosingPriceByStockCode(conn, samsung.stock_code);

	int change_hanwha = rand() % (int)((float)hanwha.price * 0.3) + 1;
	int change_samsung = rand() % (int)((float)samsung.price * 0.3) + 1;

	if (rand() % 10 < 8) { // 80% 확률
		hanwha.price = (hanwha.price + change_hanwha) > 0 ? hanwha.price + change_hanwha : hanwha.price;
		samsung.price = (samsung.price - change_samsung) > 0 ? samsung.price - change_samsung : samsung.price;
	} else { // 20% 확률
		hanwha.price = (hanwha.price - change_hanwha) > 0 ? hanwha.price - change_hanwha : hanwha.price;
		samsung.price = (samsung.price + change_samsung) > 0 ? samsung.price + change_samsung : samsung.price;
	}
	
	// 수량
	hanwha.quantity=rand()%100+1;
	samsung.quantity=rand()%100+1;
	
	updateMarketPrices(conn, &hanwha, 2);
	updateMarketPrices(conn, &samsung, 2);
}


int updateMarketPrices(MYSQL *conn, ExecutionMessage* msg, int type) { //type 1: 체결, type 2: 미체결
	int result=1;
	char query[512];
	int closing_price=0;
	int contrast=0;
	int lowest_price;
	int highest_price;
	char fluctuation_rate[11];
	char market_time[19];
	ResultStockMessageSnd data;


	format_current_time(market_time);
	strcpy(data.stock_code, msg->stock_code);
	
	
	// 전일 종가, 고가, 저가 불러오기
	if(mysql_query(conn, "select stock_code, closing_price, high_price, low_price from market_prices")) {
		finish_with_error(conn);
	}
	MYSQL_RES *sql_result = mysql_store_result(conn);
	if(sql_result==NULL) {
		finish_with_error(conn);
	}

	MYSQL_ROW row;
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
		"UPDATE market_prices "
		"SET high_price = %d "
		"WHERE stock_code = %s",
		highest_price, msg->stock_code);
	}

	else if(msg->price < lowest_price) {
		snprintf(query, sizeof(query),
		"UPDATE market_prices "
		"SET low_price = %d "
		"WHERE stock_code = %s",		
		lowest_price, msg->stock_code);
	}	
	// 쿼리 초기화
	memset(query, '\0', sizeof(query));
		
	// 대비: 현재가 - 전일종가
	contrast=msg->price-closing_price;

	// 등락률: ((현재가 - 전일종가) / 전일종가) *100
	float f = (float)((float)(msg->price - closing_price) / closing_price) *100;		
	sprintf(fluctuation_rate, "%.2f%%", f);



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


	// 메시지 보내기	
	getBalance(conn, msg->order_type, &data);
	sendResultMessage(&data);


	// 업데이트 확인 로그찍기
	// printf("[LOG] Stock Code: %s, Price: %d, Quantity: %d, Fluctuation Rate: %s, Time: %s\n", 
    //     msg->stock_code, msg->price, msg->quantity, fluctuation_rate, market_time);
	
	// 파일에 직접 기록
	FILE *log_file = fopen("/home/ec2-user/KRX/log/update_market_price.log", "a");
	if (log_file) {
		fprintf(log_file, "[MarketPriceUpdater] Stock Code: %s, Price: %d, Quantity: %d, Fluctuation Rate: %s, Time: %s\n",
				msg->stock_code, msg->price, msg->quantity, fluctuation_rate, market_time);
		fclose(log_file);
	}
	

	return result;
}