#include <ncurses.h>
#include <stdlib.h>
#include <locale.h>
#include <mysql/mysql.h>
#include <wchar.h>
#include <unistd.h>
#include "../headers/kmt_common.h"
#include "../headers/kmt_messages.h"

// 문자열을 정렬된 형태로 출력
void print_aligned(int y, int x, const char *stock_code, const char *stock_name, int price, int volume) {
    mvprintw(y, x, "%-10s", stock_code); // 종목코드 (고정 폭)
    
    // 한글 이름은 폭이 다르므로 보정
    wchar_t w_stock_name[50];
    mbstowcs(w_stock_name, stock_name, sizeof(w_stock_name) / sizeof(wchar_t));
    mvprintw(y, x + 12, "%-15ls", w_stock_name); // 종목명 (폭 보정)

    mvprintw(y, x + 30, "%-10d %-10d", price, volume); // 가격과 거래량
}

void display_market_price(MYSQL* conn) {
    int ch;
    int refresh_rate = 1000000; // 1초 (마이크로초 단위)
    while(1) {

    }
    clear(); // 화면 초기화

    // DB에서 시세 정보 가져오기
    kmt_current_market_prices data = getMarketPrice(conn);

    // 출력
    mvprintw(1, 2, "시세 정보:");
    mvprintw(2, 2, "------------------------------------------------");
    mvprintw(3, 2, "%-10s %-15s %-10s %-10s", "종목코드", "종목명", "현재가", "거래량");
    mvprintw(4, 2, "------------------------------------------------");

    // 시세 정보를 테이블 형식으로 출력
    for (int i = 0; i < 10; i++) { // 최대 10개의 데이터 출력
        if (data.body[i].stock_code[0] == '\0') break; // 데이터 끝 확인

        print_aligned(5 + i, 2, data.body[i].stock_code, data.body[i].stock_name, 
                      data.body[i].price, data.body[i].volume);
    }

    mvprintw(16, 2, "아무 키나 눌러 메뉴로 돌아갑니다...");
    refresh();
    getch(); // 사용자 입력 대기
}


void display_menu(int highlight) {
    // 메뉴 항목
    char *choices[] = {
        "1. 시세 정보 확인",
        "2. 주문 정보 확인",
        "3. 체결 정보 확인",
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
