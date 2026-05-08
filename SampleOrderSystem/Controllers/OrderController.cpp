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
        // 1. 헤더 + RESERVED 주문 목록 표시
        MainMenuView::printHeader("주문 승인/거절");
        auto reserved = ordSvc_.getOrdersByStatus(OrderStatus::RESERVED);
        OrderView::showReservedOrders(reserved);

        if (reserved.empty()) {
            MainMenuView::pause();
            break;
        }

        // 2. 번호로 주문 선택
        int sel = OrderView::promptSelectOrder((int)reserved.size());
        if (sel == 0) break;

        const Order& order = reserved[sel - 1];

        // 3. 선택된 주문 상세 + 승인/거절 선택
        MainMenuView::clearScreen();
        OrderView::showSelectedOrder(order);
        int action = OrderView::promptApproveOrReject();
        if (action == 0) continue;  // 목록으로 돌아가기

        // 4. 처리
        MainMenuView::clearScreen();
        if (action == 1) {
            if (!ordSvc_.approveOrder(order.order_number)) {
                MainMenuView::showError("승인 실패. 이미 처리된 주문일 수 있습니다.");
            } else {
                auto updated = ordSvc_.findByOrderNumber(order.order_number);
                if (updated.has_value())
                    OrderView::showApproveResult(*updated);
            }
        } else {
            auto note = OrderView::promptRejectNote();
            if (!ordSvc_.rejectOrder(order.order_number, note)) {
                MainMenuView::showError("거절 실패. 이미 처리된 주문일 수 있습니다.");
            } else {
                auto updated = ordSvc_.findByOrderNumber(order.order_number);
                if (updated.has_value())
                    OrderView::showRejectResult(*updated);
            }
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
