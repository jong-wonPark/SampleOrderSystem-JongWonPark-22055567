#include "ProductionView.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

// ── 경과 시간(시간) 계산 ─────────────────────────────────────────────

static double hoursElapsed(const std::string& iso_start) {
    if (iso_start.empty()) return 0.0;
    std::tm tm{};
    int y, mo, d, h, mi, s;
    if (sscanf_s(iso_start.c_str(), "%d-%d-%dT%d:%d:%d",
                 &y, &mo, &d, &h, &mi, &s) != 6) return 0.0;
    tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d;
    tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = s;
    tm.tm_isdst = -1;
    auto start_t = std::mktime(&tm);
    return std::difftime(std::time(nullptr), start_t) / 3600.0;
}

// ISO 8601에서 "MM-DD HH:MM" 형식으로 단축 ────────────────────────────

static std::string shortTime(const std::string& iso) {
    if (iso.size() < 16) return iso;
    // "2026-05-08T10:30:00" → "05-08 10:30"
    return iso.substr(5, 5) + " " + iso.substr(11, 5);
}

// ── 메뉴 5: 출고 처리 ────────────────────────────────────────────────

void ProductionView::showConfirmedOrders(const std::vector<Order>& orders) {
    using V = MainMenuView;
    std::cout << "\n[출고 대기 주문 목록 (CONFIRMED)]\n";
    if (orders.empty()) {
        std::cout << "  출고 대기 중인 주문이 없습니다.\n";
        return;
    }
    V::printLine('-');
    std::cout << "  "
              << V::padLeft("번호", 4)              << "  "
              << V::padRight("주문번호", 18)          << "  "
              << V::padRight("시료명", 16)            << "  "
              << V::padRight("고객명", 12)            << "  "
              << V::padLeft("수량", 4)               << "\n";
    V::printLine('-');
    for (int i = 0; i < (int)orders.size(); ++i) {
        const auto& o = orders[i];
        std::cout << "  "
                  << V::padLeft(std::to_string(i + 1), 4)              << "  "
                  << V::padRight(V::truncate(o.order_number, 18), 18)  << "  "
                  << V::padRight(V::truncate(o.sample_name, 16), 16)   << "  "
                  << V::padRight(V::truncate(o.customer_name, 12), 12) << "  "
                  << V::padLeft(std::to_string(o.order_quantity), 4)   << "\n";
    }
    V::printLine('-');
}

int ProductionView::promptSelectShipOrder(int count) {
    std::cout << "\n출고할 번호를 선택하세요 (0: 취소): ";
    std::string line;
    std::getline(std::cin, line);
    try {
        int val = std::stoi(line);
        if (val >= 0 && val <= count) return val;
    } catch (...) {}
    MainMenuView::showError("1~" + std::to_string(count) + " 또는 0을 입력하세요.");
    return MainMenuView::promptChoice(0, count,
        "1~" + std::to_string(count) + " 또는 0을 입력하세요.");
}

void ProductionView::showShipResult(const Order& order) {
    MainMenuView::showSuccess("[" + order.order_number + "] 출고 처리 완료");
    std::cout << "  → " << Clr::BrCyan << "출고 완료(RELEASE)" << Clr::Reset
              << " 상태로 전환되었습니다.\n";
}

// ── 메뉴 6: 생산 라인 ────────────────────────────────────────────────

void ProductionView::showProductionQueue(
    const std::vector<ProductionQueueItem>& queue,
    const std::vector<Order>&               orders)
{
    using V = MainMenuView;

    // 주문번호로 주문량 조회 헬퍼
    auto getOrderQty = [&](const std::string& order_number) -> int {
        for (const auto& o : orders)
            if (o.order_number == order_number) return o.order_quantity;
        return 0;
    };
    std::vector<ProductionQueueItem> inProd, waiting;
    for (const auto& p : queue) {
        if (p.status == ProductionQueueStatus::InProduction) inProd.push_back(p);
        else                                                  waiting.push_back(p);
    }

    // ── 생산 중 (InProduction) ────────────────────────────────────
    std::cout << "\n[생산 중 (InProduction)]\n";
    if (inProd.empty()) {
        std::cout << "  현재 생산 중인 항목이 없습니다.\n";
    } else {
        V::printLine('-');
        std::cout << "  "
                  << V::padLeft("번호", 4)          << "  "
                  << V::padRight("생산ID", 19)       << "  "
                  << V::padRight("주문번호", 18)      << "  "
                  << V::padRight("시료명", 14)        << "  "
                  << V::padRight("진행(생산/계획)", 14) << "  "
                  << "완료 예정\n";
        V::printLine('-');
        for (int i = 0; i < (int)inProd.size(); ++i) {
            const auto& p = inProd[i];

            // 현재까지 생산량 추정 (경과 시간 기반)
            int produced = 0;
            if (p.total_production_time_hours > 0 && p.planned_quantity > 0) {
                double avg_per = p.total_production_time_hours / p.planned_quantity;
                double elapsed = hoursElapsed(p.started_at);
                produced = std::min(
                    static_cast<int>(elapsed / avg_per), p.planned_quantity);
            }
            std::string progress = std::to_string(produced) + "/"
                                 + std::to_string(p.planned_quantity);
            std::string est = p.estimated_completion.empty()
                ? "-" : shortTime(p.estimated_completion);

            std::cout << "  "
                      << V::padLeft(std::to_string(i + 1), 4) << "  "
                      << Clr::BrYellow
                      << V::padRight(V::truncate(p.production_id, 19), 19)
                      << Clr::Reset << "  "
                      << V::padRight(V::truncate(p.order_number, 18), 18)  << "  "
                      << V::padRight(V::truncate(p.sample_name, 14), 14)   << "  "
                      << V::padRight(progress, 14)                         << "  "
                      << est << "\n";
        }
        V::printLine('-');
    }

    // ── 대기 중 (Waiting) FIFO ────────────────────────────────────
    std::cout << "\n[대기 중 (Waiting) — FIFO 순서]\n";
    if (waiting.empty()) {
        std::cout << "  대기 중인 항목이 없습니다.\n";
    } else {
        V::printLine('-');
        std::cout << "  "
                  << V::padLeft("순번", 4)            << "  "
                  << V::padRight("생산ID", 19)         << "  "
                  << V::padRight("주문번호", 18)        << "  "
                  << V::padRight("시료명", 14)          << "  "
                  << V::padLeft("주문량", 6)            << "  "
                  << V::padLeft("계획수량", 8)          << "  "
                  << V::padLeft("예상소요", 8)          << "  "
                  << "등록 시각\n";
        V::printLine('-');
        for (int i = 0; i < (int)waiting.size(); ++i) {
            const auto& p = waiting[i];
            int order_qty = getOrderQty(p.order_number);
            // 총 생산 시간을 분 단위로 표시
            int total_mins = static_cast<int>(
                std::round(p.total_production_time_hours * 60.0));
            std::ostringstream dur;
            if (total_mins >= 60) dur << (total_mins / 60) << "h" << (total_mins % 60) << "m";
            else                  dur << total_mins << "m";

            std::cout << "  "
                      << V::padLeft(std::to_string(i + 1), 4)              << "  "
                      << V::padRight(V::truncate(p.production_id, 19), 19) << "  "
                      << V::padRight(V::truncate(p.order_number, 18), 18)  << "  "
                      << V::padRight(V::truncate(p.sample_name, 14), 14)   << "  "
                      << V::padLeft(std::to_string(order_qty), 6)          << "  "
                      << V::padLeft(std::to_string(p.planned_quantity), 8) << "  "
                      << V::padLeft(dur.str(), 8)                          << "  "
                      << V::truncate(p.queued_at, 19) << "\n";
        }
        V::printLine('-');
    }
}

void ProductionView::showStartResult(const ProductionQueueItem& item) {
    MainMenuView::showSuccess("[" + item.production_id + "] 생산 시작");
    std::cout << "  → " << Clr::BrYellow << "생산 중(InProduction)" << Clr::Reset
              << " 상태로 전환되었습니다.\n";
    if (!item.estimated_completion.empty())
        std::cout << "  완료 예정: " << item.estimated_completion << "\n";
}

void ProductionView::showCompleteResult(const Order& confirmedOrder) {
    MainMenuView::showSuccess("생산 완료 처리");
    std::cout << "  → 주문 [" << confirmedOrder.order_number << "] "
              << Clr::BrGreen << "출고 대기(CONFIRMED)" << Clr::Reset
              << " 상태로 전환되었습니다.\n";
}
