#include "ProductionView.h"

#include <iostream>

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

std::string ProductionView::promptShipOrderNumber() {
    return MainMenuView::prompt("출고할 주문번호 (0: 취소)");
}

void ProductionView::showShipResult(const Order& order) {
    MainMenuView::showSuccess("[" + order.order_number + "] 출고 처리 완료");
    std::cout << "  → " << Clr::BrCyan << "출고 완료(RELEASE)" << Clr::Reset
              << " 상태로 전환되었습니다.\n";
}

int ProductionView::promptProductionSubMenu() {
    MainMenuView::printHeader("생산 라인");
    std::cout << "\n"
              << "  1. 생산 시작  (Waiting → InProduction)\n"
              << "  2. 생산 완료  (InProduction → CONFIRMED)\n"
              << "  0. 뒤로\n\n";
    MainMenuView::printLine();
    return MainMenuView::promptChoice(0, 2);
}

void ProductionView::showProductionQueue(
    const std::vector<ProductionQueueItem>& queue)
{
    using V = MainMenuView;
    std::vector<ProductionQueueItem> inProd, waiting;
    for (const auto& p : queue) {
        if (p.status == ProductionQueueStatus::InProduction) inProd.push_back(p);
        else                                                  waiting.push_back(p);
    }

    // ── 생산 중 ────────────────────────────────────────────────
    std::cout << "\n[생산 중 (InProduction)]\n";
    if (inProd.empty()) {
        std::cout << "  현재 생산 중인 항목이 없습니다.\n";
    } else {
        V::printLine('-');
        std::cout << "  "
                  << V::padRight("생산ID", 19)   << "  "
                  << V::padRight("주문번호", 18)  << "  "
                  << V::padRight("시료명", 14)    << "  "
                  << V::padLeft("수량", 4)        << "  "
                  << "시작 시각\n";
        V::printLine('-');
        for (const auto& p : inProd) {
            std::cout << "  "
                      << Clr::BrYellow
                      << V::padRight(V::truncate(p.production_id, 19), 19)
                      << Clr::Reset << "  "
                      << V::padRight(V::truncate(p.order_number, 18), 18)  << "  "
                      << V::padRight(V::truncate(p.sample_name, 14), 14)   << "  "
                      << V::padLeft(std::to_string(p.planned_quantity), 4) << "  "
                      << V::truncate(p.started_at.empty() ? "-" : p.started_at, 19)
                      << "\n";
        }
        V::printLine('-');
    }

    // ── 대기 중 ────────────────────────────────────────────────
    std::cout << "\n[대기 중 (Waiting) — FIFO 순서]\n";
    if (waiting.empty()) {
        std::cout << "  대기 중인 항목이 없습니다.\n";
    } else {
        V::printLine('-');
        std::cout << "  "
                  << V::padLeft("순번", 4)         << "  "
                  << V::padRight("생산ID", 19)      << "  "
                  << V::padRight("주문번호", 18)    << "  "
                  << V::padRight("시료명", 14)      << "  "
                  << V::padLeft("수량", 4)          << "  "
                  << "등록 시각\n";
        V::printLine('-');
        for (int i = 0; i < (int)waiting.size(); ++i) {
            const auto& p = waiting[i];
            std::cout << "  "
                      << V::padLeft(std::to_string(i + 1), 4)              << "  "
                      << V::padRight(V::truncate(p.production_id, 19), 19) << "  "
                      << V::padRight(V::truncate(p.order_number, 18), 18)  << "  "
                      << V::padRight(V::truncate(p.sample_name, 14), 14)   << "  "
                      << V::padLeft(std::to_string(p.planned_quantity), 4) << "  "
                      << V::truncate(p.queued_at, 19) << "\n";
        }
        V::printLine('-');
    }
}

void ProductionView::showStartResult(const ProductionQueueItem& item) {
    MainMenuView::showSuccess("[" + item.production_id + "] 생산 시작");
    std::cout << "  → " << Clr::BrYellow << "생산 중(InProduction)" << Clr::Reset
              << " 상태로 전환되었습니다.\n";
}

std::string ProductionView::promptCompleteProductionId() {
    return MainMenuView::prompt("생산 완료할 생산ID (0: 취소)");
}

void ProductionView::showCompleteResult(const Order& confirmedOrder) {
    MainMenuView::showSuccess("생산 완료 처리");
    std::cout << "  → 주문 [" << confirmedOrder.order_number << "] "
              << Clr::BrGreen << "출고 대기(CONFIRMED)" << Clr::Reset
              << " 상태로 전환되었습니다.\n";
}
