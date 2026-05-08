#pragma once
#include <string>

// ── 주문 상태 ──────────────────────────────────────────────────────

enum class OrderStatus {
    RESERVED,   // 주문 접수 (검토 대기 포함)
    REJECTED,   // 주문 거절
    PRODUCING,  // 승인 완료, 재고 부족으로 생산 중
    CONFIRMED,  // 승인 완료, 출고 대기 중
    RELEASE     // 출고 완료
};

// ── 생산 큐 상태 ───────────────────────────────────────────────────

enum class ProductionQueueStatus {
    Waiting,      // 생산 대기
    InProduction  // 생산 중
};

// ── 화면 표시용 (한국어) ──────────────────────────────────────────

inline std::string toString(OrderStatus s) {
    switch (s) {
        case OrderStatus::RESERVED:  return "주문 접수";
        case OrderStatus::REJECTED:  return "주문 거절";
        case OrderStatus::PRODUCING: return "생산 중";
        case OrderStatus::CONFIRMED: return "출고 대기";
        case OrderStatus::RELEASE:   return "출고 완료";
        default:                     return "알수없음";
    }
}

inline std::string toString(ProductionQueueStatus s) {
    switch (s) {
        case ProductionQueueStatus::Waiting:      return "대기";
        case ProductionQueueStatus::InProduction: return "생산 중";
        default:                                  return "알수없음";
    }
}

// ── JSON 직렬화용 (Phase 2 Persistence에서 사용) ──────────────────

inline std::string toJsonString(OrderStatus s) {
    switch (s) {
        case OrderStatus::RESERVED:  return "RESERVED";
        case OrderStatus::REJECTED:  return "REJECTED";
        case OrderStatus::PRODUCING: return "PRODUCING";
        case OrderStatus::CONFIRMED: return "CONFIRMED";
        case OrderStatus::RELEASE:   return "RELEASE";
        default:                     return "RESERVED";
    }
}

inline OrderStatus orderStatusFromJson(const std::string& s) {
    if (s == "REJECTED")  return OrderStatus::REJECTED;
    if (s == "PRODUCING") return OrderStatus::PRODUCING;
    if (s == "CONFIRMED") return OrderStatus::CONFIRMED;
    if (s == "RELEASE")   return OrderStatus::RELEASE;
    return OrderStatus::RESERVED;
}

inline std::string toJsonString(ProductionQueueStatus s) {
    return (s == ProductionQueueStatus::InProduction) ? "InProduction" : "Waiting";
}

inline ProductionQueueStatus productionQueueStatusFromJson(const std::string& s) {
    return (s == "InProduction") ? ProductionQueueStatus::InProduction
                                 : ProductionQueueStatus::Waiting;
}
