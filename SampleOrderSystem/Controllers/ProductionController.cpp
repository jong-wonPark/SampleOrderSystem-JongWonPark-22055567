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

    // InProduction 항목만 선택 대상
    std::vector<ProductionQueueItem> inProd;
    for (const auto& p : queue)
        if (p.status == ProductionQueueStatus::InProduction) inProd.push_back(p);

    if (inProd.empty()) {
        MainMenuView::showError("생산 중인 항목이 없습니다.");
        return;
    }

    int sel = ProductionView::promptSelectInProduction((int)inProd.size());
    if (sel == 0) return;

    // complete 후 큐에서 사라지므로 production_id·order_number 미리 추출
    const std::string& prodId   = inProd[sel - 1].production_id;
    const std::string& orderNum = inProd[sel - 1].order_number;

    if (!prodSvc_.completeProduction(prodId)) {
        MainMenuView::showError("생산 완료 처리 실패.");
        return;
    }

    auto order = ordSvc_.findByOrderNumber(orderNum);
    if (order.has_value())
        ProductionView::showCompleteResult(*order);
}
