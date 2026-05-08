#pragma once
#include <string>
#include "Enums.h"

struct Order {
    std::string order_number;                          // ORD-YYYYMMDD-XXXX (자동 채번)
    std::string sample_id;
    std::string sample_name;                           // 비정규화 — 표시용 캐시
    std::string customer_name;
    int         order_quantity = 0;
    OrderStatus status         = OrderStatus::RESERVED;
    std::string order_date;                            // 접수 일시 (ISO 8601)
    std::string note;                                  // 거절 사유 등 메모
};
