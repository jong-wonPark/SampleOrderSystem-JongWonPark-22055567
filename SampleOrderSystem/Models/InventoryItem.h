#pragma once
#include <string>

struct InventoryItem {
    std::string sample_id;
    int         quantity     = 0;    // 현재 재고 수량
    std::string unit         = "EA"; // 단위
    std::string last_updated;        // 최종 갱신 일시 (ISO 8601)
};
