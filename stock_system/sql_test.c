#include<stdio.h>
#include<stdlib.h>
#include "/usr/include/mysql/mysql.h"



void finish_with_error(MYSQL *con) {
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    exit(1);
}
int main() 
{
	MYSQL *conn;
	MYSQL_ROW row;

	char *server = "localhost";
	char *user="root";
	char *pw="Hanwha1!";
	char *database="hanwha_krx";

	conn = mysql_init(NULL); // connection 변수 초기화

	if (mysql_real_connect(conn, server, user, pw, database, 0, NULL, 0)==NULL){  
		finish_with_error(conn);
	}
	
	if(mysql_query(conn, "select * from market_prices m join stock_info s on m.stock_code=s.stock_code")){
		finish_with_error(conn);
	}

	MYSQL_RES *result = mysql_store_result(conn);
	

	if (result == NULL) {
        	finish_with_error(conn);
    	}

	int num_fields = mysql_num_fields(result);

	printf("Stock Code | Stock Name\n");
	printf("------------------------\n");
	while ((row = mysql_fetch_row(result))) {
        	for (int i = 0; i < num_fields; i++) {
        		printf("%s ", row[i] ? row[i] : "NULL");
        	}
		printf("\n");
	}

	mysql_free_result(result);
	mysql_close(conn);

	return 0;



}


