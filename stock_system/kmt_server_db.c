#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <mysql/mysql.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "../headers/kmt_common.h"
#include "../headers/kmt_messages.h"

#define PORT 8080
#define QUEUE_KEY 1234
#ifndef SIGTERM
#define SIGTERM 15
#endif


typedef struct  {
    long msgtype; // 1: 체결  2: 미체결 
    char stock_code[7];  // 종목 코드
    char order_type;     // 'B' (매수) or 'S' (매도)
    int price;  // 체결 가격
    int quantity; // 체결 수량
} msgbuf;

typedef struct  {
    long msgtype; // 1: 체결  2: 미체결 
    char stock_code[7];  // 종목 코드
    char order_type;     // 'B' (매수) or 'S' (매도)
    int price;  // 체결 가격
    int quantity; // 체결 수량
    char time[19]; // 체결 시간
} msgbuf_info;

int send_data(int client_socket, MYSQL* conn) {
    kmt_current_market_prices data;
    memset(&data, 0, sizeof(data));
	
    // DB에서 시세 데이터 가져오기
    data=getMarketPrice(conn);    


    // 데이터 전송
    if (send(client_socket, &data, sizeof(data), 0) < 0) {
        perror("Failed to send data");
        return 1;
    }

    return 0;
}


int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;

    //============ 메세지 큐 연결 =============
    int original_key_id;
    msgbuf msg;
    msgbuf_info msg_info;
    msg.msgtype = 1;
    msg_info.msgtype=1;
    original_key_id = msgget((key_t) QUEUE_KEY, IPC_CREAT|0666);
    if (original_key_id == -1) {
        printf("Message Get Failed!\n");
        exit(0);
    }


    //=====================================


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

    
    // ========== 자식 프로세스 생성 ===============
    pid_t pid= fork();
    if(pid<0) {
        perror("Fork Failed");
        exit(EXIT_FAILURE);
    }

    if(pid==0) {
        // 자식 프로세스: 메시지 큐 처리
        while (1) {
            // 체결 메시지
            if (msgrcv(original_key_id, &msg, sizeof(msg), 1, IPC_NOWAIT) != -1) {
                
                // 시세 업데이트
                int update_result = updateMarketPrices(conn, &msg, 1);
                if (update_result == 0) {
                    printf("[NO UPDATE MESSAGE 1]\n");
                } else {
                    printf("[UPDATED MESSAGE 1]\n");
                }
            }
            // 미체결 메시지
            if (msgrcv(original_key_id, &msg, sizeof(msg), 2, IPC_NOWAIT) != -1) {
                int update_result = updateMarketPrices(conn, &msg, 2);
                if (update_result == 0) {
                    printf("[NO UPDATE MESSAGE 2]\n");
                } else {
                    printf("[UPDATED MESSAGE 2]\n");
                }
            }
            sleep(1); // 메시지 큐 체크 주기
        }

    } else {

        // 소켓 연결
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
        

        // 부모 프로세스: 클라이언트와 데이터 송수신 처리
        int send_result = 0;
        while (1) {
            if (send_result == 1) break;
            send_result = send_data(client_socket, conn);
            sleep(5); // 5초마다 데이터 전송
        }
        close(client_socket);
        close(server_fd);

        // 자식 프로세스 종료 대기
        kill(pid, SIGTERM);
    }

    // free MYSQL conn 해주기
    mysql_close(conn);


    return 0;
}

