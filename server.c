#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "structures/kmt_messages.h"

#define PORT 8080

void send_kmt_current_market_price(int client_socket, kmt_current_market_prices *prices);

int main() 
{
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // 소케 파일 디스크립터 생성
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) ==0 ) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }


    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 주소 및 포트 바인딩
    if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 연결 대기
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }


    printf("Waiting for a client connection on port %d...\n", PORT);

    // 클라이언트 연결 수락
    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected.\n");

    // 주식 데이터 초기화
    kmt_current_market_prices my_stocks = {
        .header = {.tr_id = "TR01", .length = 4},
        .body = {
            {"AAPL", "Apple Inc.", 145.32, 1000000, 5, "+3.56%", {{1, 145, 500}, {2, 144, 400}}, 146, 143, "2025-01-20 14:35:00"},
            {"GOOG", "Alphabet Inc.", 2732.10, 800000, -20, "-0.73%", {{1, 2732, 600}, {2, 2730, 350}}, 2750, 2700, "2025-01-20 14:36:00"},
            {"AMZN", "Amazon.com Inc.", 3315.50, 1200000, 15, "+0.45%", {{1, 3315, 700}, {2, 3310, 450}}, 3330, 3290, "2025-01-20 14:37:00"},
            {"MSFT", "Microsoft Corp.", 299.80, 900000, 10, "+3.45%", {{1, 300, 650}, {2, 298, 420}}, 302, 297, "2025-01-20 14:38:00"}
        }
    };

    // 5초마다 데이터를 전송
    while (1) {
        send_stock_data(client_socket, &my_stocks);
        sleep(5);
    }

    close(client_socket);
    close(server_fd);
    return 0;

}