#include "ProductionController.h"
#include "../Views/ProductionView.h"
#include "../Views/MainMenuView.h"

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
        int choice = ProductionView::promptProductionSubMenu();
        if (choice == 0) break;
        MainMenuView::clearScreen();
        switch (choice) {
            case 1: handleStartProduction();    break;
            case 2: handleCompleteProduction(); break;
        }
        MainMenuView::pause();
    }
}

void ProductionController::handleShip() {
    auto confirmed = ordSvc_.getOrdersByStatus(OrderStatus::CONFIRMED);
    ProductionView::showConfirmedOrders(confirmed);
    if (confirmed.empty()) return;

    auto numStr = ProductionView::promptShipOrderNumber();
    if (numStr == "0" || numStr.empty()) return;

    if (!prodSvc_.shipOrder(numStr)) {
        MainMenuView::showError("출고 실패. CONFIRMED 상태의 주문번호인지 확인하세요.");
        return;
    }

    auto updated = ordSvc_.findByOrderNumber(numStr);
    if (updated.has_value())
        ProductionView::showShipResult(*updated);
}

void ProductionController::handleStartProduction() {
    ProductionView::showProductionQueue(prodSvc_.getQueue());

    if (!prodSvc_.startNextProduction()) {
        MainMenuView::showError("대기 중인 생산 항목이 없습니다.");
        return;
    }

    auto current = prodSvc_.getCurrentProduction();
    if (current.has_value())
        ProductionView::showStartResult(*current);
}

void ProductionController::handleCompleteProduction() {
    auto queue = prodSvc_.getQueue();
    ProductionView::showProductionQueue(queue);

    auto prodId = ProductionView::promptCompleteProductionId();
    if (prodId == "0" || prodId.empty()) return;

    // complete 후 큐에서 사라지므로 order_number를 미리 추출
    std::string orderNum;
    for (const auto& p : queue)
        if (p.production_id == prodId) { orderNum = p.order_number; break; }

    if (orderNum.empty()) {
        MainMenuView::showError("생산ID를 찾을 수 없습니다: " + prodId);
        return;
    }

    if (!prodSvc_.completeProduction(prodId)) {
        MainMenuView::showError("생산 완료 실패. InProduction 상태인지 확인하세요.");
        return;
    }

    auto order = ordSvc_.findByOrderNumber(orderNum);
    if (order.has_value())
        ProductionView::showCompleteResult(*order);
}
