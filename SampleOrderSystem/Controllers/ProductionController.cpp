#include "ProductionController.h"
#include "../Views/ProductionView.h"
#include "../Views/MainMenuView.h"

#include <algorithm>
#include <iostream>

ProductionController::ProductionController(ProductionService& prodSvc, OrderService& ordSvc)
    : prodSvc_(prodSvc), ordSvc_(ordSvc)
{}

void ProductionController::runShipping() {
    MainMenuView::clearScreen();
    handleShip();
    MainMenuView::pause();
}

void ProductionController::runProduction() {
    while (true) {
        MainMenuView::printHeader("생산 라인");

        // 완료 예정 시간이 지난 InProduction 항목 자동 완료
        auto completedOrders = prodSvc_.autoCompleteExpired();
        for (const auto& orderNum : completedOrders) {
            auto order = ordSvc_.findByOrderNumber(orderNum);
            if (order.has_value())
                ProductionView::showCompleteResult(*order);
        }
        if (!completedOrders.empty())
            MainMenuView::pause();

        // 현재 처리 중인 내역 + 대기 중인 내역 표시
        auto queue = prodSvc_.getQueue();
        ProductionView::showProductionQueue(queue);

        // 대기 항목 존재 여부로 옵션 결정
        bool hasWaiting = std::any_of(queue.begin(), queue.end(),
            [](const ProductionQueueItem& p) {
                return p.status == ProductionQueueStatus::Waiting;
            });

        std::cout << "\n";
        if (hasWaiting)
            std::cout << "  1. 생산 시작  (Waiting → InProduction)\n";
        std::cout << "  0. 뒤로\n\n";
        MainMenuView::printLine();

        int choice = MainMenuView::promptChoice(0, hasWaiting ? 1 : 0);
        if (choice == 0) break;

        MainMenuView::clearScreen();
        handleStartProduction();
        MainMenuView::pause();
        MainMenuView::clearScreen();
    }
}

void ProductionController::handleShip() {
    auto confirmed = ordSvc_.getOrdersByStatus(OrderStatus::CONFIRMED);
    ProductionView::showConfirmedOrders(confirmed);
    if (confirmed.empty()) return;

    int sel = ProductionView::promptSelectShipOrder((int)confirmed.size());
    if (sel == 0) return;

    const std::string& orderNum = confirmed[sel - 1].order_number;

    if (!prodSvc_.shipOrder(orderNum)) {
        MainMenuView::showError("출고 처리 실패.");
        return;
    }

    auto updated = ordSvc_.findByOrderNumber(orderNum);
    if (updated.has_value())
        ProductionView::showShipResult(*updated);
}

void ProductionController::handleStartProduction() {
    if (!prodSvc_.startNextProduction()) {
        MainMenuView::showError("대기 중인 생산 항목이 없습니다.");
        return;
    }

    auto current = prodSvc_.getCurrentProduction();
    if (current.has_value())
        ProductionView::showStartResult(*current);
}
