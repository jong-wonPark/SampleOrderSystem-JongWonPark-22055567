#include "OrderController.h"
#include "../Views/OrderView.h"
#include "../Views/MainMenuView.h"

OrderController::OrderController(OrderService& ordSvc, InventoryService& invSvc)
    : ordSvc_(ordSvc), invSvc_(invSvc)
{}

void OrderController::runPlaceOrder() {
    MainMenuView::clearScreen();
    handlePlaceOrder();
    MainMenuView::pause();
}

void OrderController::runApproval() {
    while (true) {
        int choice = OrderView::promptApprovalSubMenu();
        if (choice == 0) break;
        MainMenuView::clearScreen();
        switch (choice) {
            case 1: handleApprove(); break;
            case 2: handleReject();  break;
        }
        MainMenuView::pause();
    }
}

void OrderController::handlePlaceOrder() {
    auto input = OrderView::promptOrderInput();

    auto sample = invSvc_.findSampleById(input.sample_id);
    if (!sample.has_value()) {
        MainMenuView::showError("등록되지 않은 시료 ID입니다: " + input.sample_id);
        return;
    }

    auto order = ordSvc_.placeOrder(
        input.sample_id, sample->sample_name,
        input.customer_name, input.order_quantity);
    OrderView::showPlaceOrderResult(order);
}

void OrderController::handleApprove() {
    auto reserved = ordSvc_.getOrdersByStatus(OrderStatus::RESERVED);
    OrderView::showReservedOrders(reserved);
    if (reserved.empty()) return;

    auto numStr = OrderView::promptOrderNumber("승인");
    if (numStr == "0" || numStr.empty()) return;

    if (!ordSvc_.approveOrder(numStr)) {
        MainMenuView::showError("승인 실패. RESERVED 상태의 주문번호인지 확인하세요.");
        return;
    }

    auto updated = ordSvc_.findByOrderNumber(numStr);
    if (updated.has_value())
        OrderView::showApproveResult(*updated);
}

void OrderController::handleReject() {
    auto reserved = ordSvc_.getOrdersByStatus(OrderStatus::RESERVED);
    OrderView::showReservedOrders(reserved);
    if (reserved.empty()) return;

    auto numStr = OrderView::promptOrderNumber("거절");
    if (numStr == "0" || numStr.empty()) return;

    auto note = OrderView::promptRejectNote();

    if (!ordSvc_.rejectOrder(numStr, note)) {
        MainMenuView::showError("거절 실패. RESERVED 상태의 주문번호인지 확인하세요.");
        return;
    }

    auto updated = ordSvc_.findByOrderNumber(numStr);
    if (updated.has_value())
        OrderView::showRejectResult(*updated);
}
