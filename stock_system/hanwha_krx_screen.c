#include <ncurses.h>
#include <stdlib.h>
#include <locale.h>
#include <mysql/mysql.h>
#include <wchar.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>
#include "../headers/kmt_common.h"
#include "../headers/kmt_messages.h"
#define MAX_MSG_SIZE 512
#define QUEUE_KEY 5678
#define LOG_FILE "/home/ec2-user/KRX/log/update_market_price.log"


// 메시지 구조체 정의
typedef struct {
    long msgtype; // 메시지 유형
    char stock_code[7];
    char order_type;
    int price;
    int quantity;
} msgbuf;



// 문자열을 정렬된 형태로 출력
void print_aligned(int y, int x, const char *stock_code, const char *stock_name, int price, int volume) {
    mvprintw(y, x, "%-10s", stock_code); // 종목코드 (고정 폭)
    
    // 한글 이름은 폭이 다르므로 보정
    wchar_t w_stock_name[50];
    mbstowcs(w_stock_name, stock_name, sizeof(w_stock_name) / sizeof(wchar_t));
    mvprintw(y, x + 12, "%-15ls", w_stock_name); // 종목명 (폭 보정)

    mvprintw(y, x + 30, "%-10d %-10d", price, volume); // 가격과 거래량
}
// 1. 시세 정보 출력
void display_market_price(MYSQL* conn) {
    struct timeval timeout; // select() 대기 시간 설정
    int ch;
    int refresh_rate = 1; // 새로고침 주기 (초 단위)

    while (1) {
        clear(); // 화면 초기화

        // DB에서 시세 정보 가져오기
        kmt_current_market_prices data = getMarketPrice(conn);

        // 시세 정보 출력
        mvprintw(1, 2, "시세 정보:");
        mvprintw(2, 2, "------------------------------------------------");
        mvprintw(3, 3, "%-10s %-15s %-10s %-10s", "종목코드", "종목명", "현재가", "거래량");
        mvprintw(4, 2, "------------------------------------------------");

        for (int i = 0; i < 4; i++) { // 최대 4개의 데이터 출력
            if (data.body[i].stock_code[0] == '\0') break; // 데이터 끝 확인
            mvprintw(5 + i, 2, "%-10s %-15s %-10d %-10d",
                     data.body[i].stock_code, data.body[i].stock_name,
                     data.body[i].price, data.body[i].volume);
        }

        mvprintw(16, 2, "1초마다 데이터가 새로고침됩니다. 종료하려면 'q'를 누르세요.");
        refresh();

        // 입력 대기 (select를 사용하여 효율적으로 처리)
        timeout.tv_sec = refresh_rate; // 초 단위
        timeout.tv_usec = 0; // 마이크로초 단위
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        // 입력 또는 타임아웃 대기
        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout);

        if (ret > 0) {
            ch = getch();
            if (ch == 'q' || ch == 'Q') {
                break; // 루프 종료
            }
        }
    }
}

// 2. 체결내역 출력
void display_execution() {
    FILE *log_file;
    int refresh_rate = 1; // 새로고침 주기 (초 단위)
    int ch;
    
    char **stored_lines = NULL; // 동적 로그 저장 배열
    int line_count = 0; // 저장된 로그 개수
    int scroll_offset = 0; // 화면 스크롤 오프셋

    while (1) {
        log_file = fopen(LOG_FILE, "r");
        if (!log_file) {
            mvprintw(1, 2, "로그 파일을 열 수 없습니다.");
            refresh();
            getch();
            return;
        }

        // 기존 로그 메모리 해제
        if (stored_lines) {
            for (int i = 0; i < line_count; i++) {
                free(stored_lines[i]);
            }
            free(stored_lines);
        }

        stored_lines = NULL;
        line_count = 0;

        // 파일에서 로그 읽기
        char line[1024];
        while (fgets(line, sizeof(line), log_file)) {
            if (strstr(line, "[MarketPriceUpdater]")) { // 로그 필터링
                stored_lines = realloc(stored_lines, (line_count + 1) * sizeof(char *));
                stored_lines[line_count] = strdup(line);
                line_count++;
            }
        }
        fclose(log_file);

        // 화면 출력
        clear();
        mvprintw(1, 2, "체결 내역 (스크롤 가능, ↑/↓ 이동, 'q' 종료)");
        mvprintw(2, 2, "-------------------------------------------------------------");
        mvprintw(3, 5, "%-15s %-10s %-10s %-10s %-20s", "Stock Code", "Price", "Quantity", "Fluctuation", "Time");
        mvprintw(4, 2, "-------------------------------------------------------------");

        int row = 5;
        for (int i = scroll_offset; i < line_count && row < LINES - 2; i++) {
            char stock_code[7], fluctuation_rate[11], market_time[19];
            int price, quantity;

            // 저장된 로그 파싱
            if (sscanf(stored_lines[i],
                       "[MarketPriceUpdater] Stock Code: %6s, Price: %d, Quantity: %d, Fluctuation Rate: %10[^,], Time: %14s",
                       stock_code, &price, &quantity, fluctuation_rate, market_time) == 5) {
                mvprintw(row++, 2, "%-15s %-10d %-10d %-15s %-15s",
                         stock_code, price, quantity, fluctuation_rate, market_time);
            } else {
                mvprintw(row++, 2, "파싱 실패: %s", stored_lines[i]);
            }
        }

        mvprintw(LINES - 1, 2, "↑/↓ 키로 스크롤, 'q' 키로 종료");
        refresh();

        // 입력 대기 및 새로고침 대기
        struct timeval timeout;
        timeout.tv_sec = refresh_rate; // 초 단위
        timeout.tv_usec = 0; // 마이크로초 단위

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout);
        if (ret > 0) {
            ch = getch();
            if (ch == 'q' || ch == 'Q') {
                break; // 루프 종료
            } else if (ch == KEY_DOWN && scroll_offset < line_count - (LINES - 6)) {
                scroll_offset++;
            } else if (ch == KEY_UP && scroll_offset > 0) {
                scroll_offset--;
            }
        }
    }

    // 동적 할당된 메모리 해제
    if (stored_lines) {
        for (int i = 0; i < line_count; i++) {
            free(stored_lines[i]);
        }
        free(stored_lines);
    }
}


void display_menu(int highlight) {
    // 메뉴 항목
    char *choices[] = {
        "1. 시세 정보 확인",
        "2. 체결 정보 확인",
        "3. 주문 정보 확인",
        "4. 종료"
    };
    int n_choices = sizeof(choices) / sizeof(choices[0]);

    clear(); // 화면 초기화
    mvprintw(1, 2, "메뉴를 선택하세요:");
    for (int i = 0; i < n_choices; i++) {
        if (i + 1 == highlight) {
            attron(A_REVERSE); // 강조 표시
            mvprintw(3 + i, 4, choices[i]);
            attroff(A_REVERSE);
        } else {
            mvprintw(3 + i, 4, choices[i]);
        }
    }
    refresh();
}

int main() {
    int highlight = 1; // 강조 표시된 메뉴
    int choice = 0;    // 선택한 메뉴
    int c;

    // MySQL 연결 초기화
    MYSQL* conn = connect_to_mysql();

    // UTF-8 환경 설정
    setlocale(LC_ALL, "ko_KR.UTF-8");

    initscr();          // ncurses 초기화
    cbreak();           // 입력을 버퍼링 없이 처리
    noecho();           // 입력된 문자를 화면에 표시하지 않음
    keypad(stdscr, TRUE); // 화살표 키 활성화
    curs_set(0);        // 커서 숨기기

    while (1) {
        display_menu(highlight);
        c = getch();

        switch (c) {
            case KEY_UP: // 위쪽 화살표 키
                if (highlight > 1) {
                    highlight--;
                }
                break;
            case KEY_DOWN: // 아래쪽 화살표 키
                if (highlight < 4) {
                    highlight++;
                }
                break;
            case '\n': // Enter 키
                choice = highlight;
                if (choice == 4) {
                    endwin(); // ncurses 종료
                    mysql_close(conn); // MySQL 연결 종료
                    printf("프로그램을 종료합니다.\n");
                    return 0;
                } else if (choice == 1) {
                    // 시세 정보 출력
                    display_market_price(conn);
                } else if(choice==2)  {
                    display_execution();
                } else {
                    clear();
                    mvprintw(5, 5, "선택한 메뉴: %d (기능 미구현)", choice);
                    mvprintw(7, 5, "아무 키나 눌러 메뉴로 돌아갑니다...");
                    refresh();
                    getch();
                }
                break;
            default:
                break;
        }
    }

    endwin(); // ncurses 종료
    mysql_close(conn); // MySQL 연결 종료
    return 0;
}
