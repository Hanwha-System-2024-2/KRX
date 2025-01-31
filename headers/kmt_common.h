#ifndef KMT_COMMON_H
#define KMT_COMMON_H

#include "krx_messages.h"
#include<mysql/mysql.h>

// 시세 조회
kmt_current_market_prices getMarketPrice(MYSQL *conn);

// 종목 정보 조회
kmt_stock_infos getStockInfo(MYSQL *conn);

// 시세 업데이트
int updateMarketPrices();

// 디비 연결
MYSQL *connect_to_mysql();
#endif
