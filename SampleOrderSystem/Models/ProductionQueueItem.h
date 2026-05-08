#pragma once
#include <string>
#include "Enums.h"

struct ProductionQueueItem {
    std::string           production_id;                            // PROD-YYYYMMDD-XXXX
    std::string           order_number;                             // 연결된 주문번호
    std::string           sample_id;
    std::string           sample_name;
    int                   planned_quantity = 0;
    ProductionQueueStatus status           = ProductionQueueStatus::Waiting;
    std::string           queued_at;        // 큐 등록 일시 (ISO 8601) — FIFO 정렬 기준
    std::string           started_at;       // 생산 시작 일시 (빈 문자열 = 미시작)
};
