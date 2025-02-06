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
#include <sys/select.h>
#include <errno.h>
#include "../headers/kmt_common.h"
#include "../headers/krx_messages.h"
#include "../headers/kft_ipc.h"

#define PORT 8080
#ifndef SIGTERM
#define SIGTERM 15
#endif




typedef struct  {
    long msgtype; // 1: 체결  2: 미체결 
    char stock_code[7];  // 종목 코드
    char order_type;     // 'B' (매수) or 'S' (매도)
    int price;  // 체결 가격
    int quantity; // 체결 수량
    char time[15]; // 체결 시간
} ExecutionMessageInfo;

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

int recv_data(int client_socket, MYSQL* conn) {
    fd_set read_fds;
    struct timeval timeout;
    char buffer[512];
    
    // init 
    memset(buffer, 0, sizeof(buffer));
    FD_ZERO(&read_fds);
    FD_SET(client_socket, &read_fds);

    timeout.tv_sec=5;
    timeout.tv_usec=0;


    int activity = select(client_socket + 1, &read_fds, NULL, NULL, &timeout);
    if (activity < 0) {
        perror("select failed");
        return 1;
    } else if (activity == 0) {
        printf("Waiting for login signal...\n");
        return 0;
    }

    // 클라이언트 데이터 수신
    memset(buffer, 0, sizeof(buffer));
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received < 0) {
        perror("recv failed");
        return 1;
    } else if (bytes_received == 0) {
        printf("Client disconnected\n");
        return 1;
    }

    // 수신한 데이터 처리
    printf("Received: %s\n", buffer);
    
    
    // DB에서 종목 데이터 가져오기
    kmt_stock_infos data;
    memset(&data, 0, sizeof(data));
    data=getStockInfo(conn);
    printf("[종목 정보 전송] stock code: %s, stock name: %s\n", data.body->stock_code, data.body->stock_name);
    // 데이터 전송
    if (send(client_socket, &data, sizeof(data), 0) < 0) {
        perror("Failed to send data");
        return 1;
    }
    
    return 0; // 성공적으로 데이터 수신
}

void handle_client_recv(int client_socket, MYSQL* conn) {
    while (1) {
        int recv_result = recv_data(client_socket, conn);
        if (recv_result == 1) break; // 클라이언트 종료 시 루프 종료
    }
    mysql_close(conn);
    close(client_socket);
    exit(0);  // 자식 프로세스 종료
}

// 서버 초기화
int init_server_socket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("[Server] Waiting for client connections...\n");
    return server_fd;
}

int init_message_queue(int key_id) {
    int queue_id = msgget((key_t)key_id, IPC_CREAT | 0666);
    if (queue_id == -1) {
        perror("Message Queue Get Failed");
        exit(EXIT_FAILURE);
    }
    return queue_id;
}

int main() {
    //============ 메세지 큐 연결 =============
    int original_key_id=init_message_queue(STOCK_SYSTEM_QUEUE_ID);
    ExecutionMessage msg;
    ExecutionMessageInfo msg_info;
    msg.msgtype = 1;
    msg_info.msgtype=1;
    

    //================== 소켓 연결 ====================
    struct sockaddr_in server_addr, client_addr;
    int server_fd=init_server_socket();
    int client_socket;
    socklen_t addr_len = sizeof(client_addr);
    
    
    // ========== 자식 프로세스 생성 ===============
    pid_t msg_queue_pid= fork();
    if(msg_queue_pid<0) {
        perror("Fork Failed");
        exit(EXIT_FAILURE);
    }
    
    // 자식 프로세스: 메시지 큐 처리
    if(msg_queue_pid==0) {
        
        // MYSQL 디비 연결하기 
        MYSQL* msg_conn=connect_to_mysql();

        while (1) {
            // 우선 순위
            if (msgrcv(original_key_id, &msg, sizeof(msg), 1, IPC_NOWAIT) != -1) {
                
                // 시세 업데이트
                int update_result = updateMarketPrices(msg_conn, &msg, msg.exectype);
                if (update_result == 0) {
                    printf("[NO UPDATE MESSAGE 1]\n");
                } else {
                    printf("[UPDATED MESSAGE 1]\n");
                }
            }
            // 일반 메시지
            if (msgrcv(original_key_id, &msg, sizeof(msg), 2, IPC_NOWAIT) != -1) {
                int update_result = updateMarketPrices(msg_conn, &msg, msg.exectype);
                if (update_result == 0) {
                    printf("[NO UPDATE MESSAGE 2]\n");
                } else {
                    printf("[UPDATED MESSAGE 2]\n");
                }
            }
            // 후순위 메시지
            if (msgrcv(original_key_id, &msg, sizeof(msg), 3, IPC_NOWAIT) != -1) {
                int update_result = updateMarketPrices(msg_conn, &msg, msg.exectype);
                if (update_result == 0) {
                    printf("[NO UPDATE MESSAGE 2]\n");
                } else {
                    printf("[UPDATED MESSAGE 2]\n");
                }
            }
            sleep(1); // 메시지 큐 체크 주기
        }
        // free MYSQL conn 해주기
        mysql_close(msg_conn);

    } else { // TCP 통신
        
        
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }
        printf("Client connected\n");
        
        
        
        // 수신 프로세스 생성 : 로그인 정보를 받고, 종목 정보 데이터를 전송하는 프로세스
        pid_t recv_pid = fork();
        if (recv_pid < 0) {
            perror("Fork failed");
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        if (recv_pid == 0) {
            MYSQL *stock_conn = connect_to_mysql();
            handle_client_recv(client_socket, stock_conn);
            mysql_close(stock_conn);
        }
        else{
            MYSQL *conn = connect_to_mysql();
            // 부모 프로세스: 클라이언트와 데이터 시세 데이터 5 초간격 송신 처리
            int send_result = 0;
            
            while (1) {
                send_result = send_data(client_socket, conn);
                if (send_result == 1) break;
                // 랜덤 시세 변경 함수
                updateMarketPricesAuto(conn);
                sleep(2); // 2초마다 데이터 전송
            }
            close(client_socket);
            close(server_fd);
            mysql_close(conn);
            // 자식 프로세스 종료 대기
            kill(msg_queue_pid, SIGTERM);
            kill(recv_pid, SIGTERM);
        }
        
    }


    return 0;
}

