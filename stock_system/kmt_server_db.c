#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include "../headers/kmt_common.h"
#include "../headers/kmt_messages.h"
#include <mysql/mysql.h>

#define PORT 8080

// for Message queue
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>


typedef struct  {
    long msgtype; // 1: 체결  2: 미체결 
    char stock_code[7];  // 종목 코드
    char order_type;     // 'B' (매수) or 'S' (매도)
    int price;  // 체결 가격
    int quantity; // 체결 수량
} msgbuf;



int send_data(int client_socket, MYSQL* conn) {
    kmt_current_market_prices data;

    // 데이터 초기화
    data.header.tr_id=8;
    data.header.length = sizeof(data.body);
     
    // DB에서 시세 데이터 가져오기
    data=getMarketPrice(conn);    


    // 데이터 전송
    if (send(client_socket, &data, sizeof(data), 0) < 0) {
        perror("Failed to send data");
        return 1;
    }

    // 종료 신호 전송
    char end_signal = '\n';
    if (send(client_socket, &end_signal, sizeof(end_signal), 0) < 0) {
        perror("Failed to send end signal");
        return 1;
    }
    return 0;
}


int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;

    ////// 메세지 큐 연결
    int key_id;
    msgbuf msg;
    msg.msgtype = 1;
    key_id = msgget((key_t) 1234, IPC_CREAT|0666); // Create Message (message queue key, message flag)
    if (key_id == -1) {
        printf("Message Get Failed!\n");
        exit(0);
    }

    ///////////


    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // MYSQL 디비 연결하기 

    MYSQL* conn=connect_to_mysql();



    // SO_REUSEADDR 설정
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for connections...\n");

    client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");
    int send_result=0;
    while (1) {
	if(send_result==1) break;
	////// 메세지 큐 받기
	if (msgrcv(key_id, &msg, sizeof(msg), 1, IPC_NOWAIT) != -1) { // Receive if msgtype is 1

	    // 체결 메시지 받으면 db 업데이트하기
	    int update_result = updateMarketPrices(conn, msg); 
	    // 
	    if (update_result==0) {
	    	printf("update failed\n");
	    }

        }
	//printf("stock_code: %s\n", msg.stock_code);
        //printf("order type : %d\n", msg.order_type);
        //printf("price : %d\n", msg.price);
	/////
        send_result=send_data(client_socket, conn);
        sleep(5); // 5초마다 데이터 전송
    }

    close(client_socket);
    close(server_fd);


    // free MYSQL conn 해주기
    mysql_close(conn);


    return 0;
}

