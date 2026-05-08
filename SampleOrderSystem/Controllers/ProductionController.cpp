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
    // 1. 완료 예정 시간이 지난 InProduction 항목 자동 완료
    for (const auto& orderNum : prodSvc_.autoCompleteExpired()) {
        auto order = ordSvc_.findByOrderNumber(orderNum);
        if (order.has_value())
            ProductionView::showCompleteResult(*order);
    }

    // 2. InProduction이 없으면 대기 항목 자동 처리
    //    shortage=0인 항목은 즉시 CONFIRMED 처리하고 다음 항목으로 진행
    {
        auto queue = prodSvc_.getQueue();
        bool hasInProd = std::any_of(queue.begin(), queue.end(),
            [](const ProductionQueueItem& p) {
                return p.status == ProductionQueueStatus::InProduction; });

        if (!hasInProd) {
            while (prodSvc_.startNextProduction()) {
                auto cur = prodSvc_.getCurrentProduction();
                if (cur.has_value()) {
                    // 실제 생산 시작됨
                    ProductionView::showStartResult(*cur);
                    break;
                }
                // shortage=0이었으므로 즉시 CONFIRMED — 다음 항목 처리 시도
            }
        }
    }

    // 3. 현재 처리 중인 내역 + 대기 중인 내역 표시
    MainMenuView::printHeader("생산 라인");
    ProductionView::showProductionQueue(prodSvc_.getQueue(), ordSvc_.getAllOrders());

    // 4. Enter → 뒤로가기
    std::cout << "\n";
    MainMenuView::pause();
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
