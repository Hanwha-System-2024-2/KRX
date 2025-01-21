#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include "../structures/kmt_messages.h"

#define PORT 8080

void format_current_time(char *buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, 15, "%Y%m%d%H%M%S", t); // "yyyymmddhhmmss"
}

void send_data(int client_socket) {
    kmt_current_market_prices data;

    // 데이터 초기화
    strcpy(data.header.tr_id, "TR01");
    data.header.length = sizeof(data.body);

    // 데이터 예시
    for (int i = 0; i < 4; i++) {
        sprintf(data.body[i].stock_code, "%06d", i + 1); // 예: "000001"
        sprintf(data.body[i].stock_name, "Stock%d", i + 1);
        data.body[i].price = 1000.0 + i * 100;
        data.body[i].volume = 1000 * (i + 1);
        data.body[i].change = i * 10;
        sprintf(data.body[i].rate_of_change, "+%d.0%%", i);
        data.body[i].hoga[0].trading_type = 1;
        data.body[i].hoga[0].price = 1000 + i * 10;
        data.body[i].hoga[0].balance = 100;
        data.body[i].hoga[1].trading_type = 2;
        data.body[i].hoga[1].price = 1100 + i * 10;
        data.body[i].hoga[1].balance = 200;
        data.body[i].high_price = 1200 + i * 10;
        data.body[i].low_price = 900 + i * 10;
        format_current_time(data.body[i].market_time);
    }

    // 데이터 전송
    if (send(client_socket, &data, sizeof(data), 0) < 0) {
        perror("Failed to send data");
    }

    // 종료 신호 전송
    char end_signal = '\n';
    if (send(client_socket, &end_signal, sizeof(end_signal), 0) < 0) {
        perror("Failed to send end signal");
    }
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

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

    while (1) {
        send_data(client_socket);
        sleep(5); // 5초마다 데이터 전송
    }

    close(client_socket);
    close(server_fd);

    return 0;
}

