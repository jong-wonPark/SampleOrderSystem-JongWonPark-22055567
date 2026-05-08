#include "OrderView.h"

#include <iostream>

OrderInput OrderView::promptOrderInput() {
    MainMenuView::printHeader("시료 주문 접수");
    std::cout << '\n';
    OrderInput in;
    in.sample_id      = MainMenuView::prompt("시료ID");
    in.customer_name  = MainMenuView::prompt("고객명");
    in.order_quantity = MainMenuView::promptInt("주문 수량");
    MainMenuView::printLine();
    return in;
}

void OrderView::showPlaceOrderResult(const Order& order) {
    MainMenuView::showSuccess("주문이 접수되었습니다.");
    std::cout << "  주문번호 : " << order.order_number  << "\n"
              << "  시료     : " << order.sample_name   << "\n"
              << "  고객     : " << order.customer_name << "\n"
              << "  수량     : " << order.order_quantity << "\n"
              << "  상태     : " << MainMenuView::colorStatus(order.status) << "\n";
}

void OrderView::showReservedOrders(const std::vector<Order>& orders) {
    using V = MainMenuView;
    std::cout << "\n[검토 대기 주문 목록 (RESERVED)]\n";
    if (orders.empty()) {
        std::cout << "  검토 대기 중인 주문이 없습니다.\n";
        return;
    }
    V::printLine('-');
    std::cout << "  "
              << V::padLeft("번호", 4)              << "  "
              << V::padRight("주문번호", 18)          << "  "
              << V::padRight("시료명", 16)            << "  "
              << V::padRight("고객명", 12)            << "  "
              << V::padLeft("수량", 4)               << "  "
              << "접수 일시\n";
    V::printLine('-');
    for (int i = 0; i < (int)orders.size(); ++i) {
        const auto& o = orders[i];
        std::cout << "  "
                  << V::padLeft(std::to_string(i + 1), 4)              << "  "
                  << V::padRight(V::truncate(o.order_number, 18), 18)  << "  "
                  << V::padRight(V::truncate(o.sample_name, 16), 16)   << "  "
                  << V::padRight(V::truncate(o.customer_name, 12), 12) << "  "
                  << V::padLeft(std::to_string(o.order_quantity), 4)   << "  "
                  << V::truncate(o.order_date, 19) << "\n";
    }
    V::printLine('-');
}

int OrderView::promptSelectOrder(int count) {
    std::cout << "\n처리할 번호를 선택하세요 (0: 뒤로): " << std::flush;
    // promptChoice는 "선택: " 접두사를 출력하지만 여기선 이미 별도 안내문을 출력했으므로
    // 첫 입력만 직접 처리하고, 재시도는 promptChoice에 위임한다
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

void OrderView::showSelectedOrder(const Order& order) {
    using V = MainMenuView;
    std::cout << "\n[선택된 주문]\n";
    V::printLine('-');
    std::cout << "  주문번호 : " << order.order_number  << "\n"
              << "  시료     : " << order.sample_name   << "\n"
              << "  고객     : " << order.customer_name << "\n"
              << "  수량     : " << order.order_quantity << " EA\n"
              << "  접수일시 : " << order.order_date     << "\n";
    V::printLine('-');
}

int OrderView::promptApproveOrReject() {
    std::cout << "\n"
              << "  1. 승인\n"
              << "  2. 거절\n"
              << "  0. 뒤로\n\n";
    MainMenuView::printLine();
    return MainMenuView::promptChoice(0, 2);
}

std::string OrderView::promptRejectNote() {
    return MainMenuView::prompt("거절 사유 (선택, 빈 칸 가능)");
}

void OrderView::showApproveResult(const Order& order) {
    MainMenuView::showSuccess("주문 승인 완료: " + order.order_number);
    if (order.status == OrderStatus::CONFIRMED) {
        std::cout << "  → 재고 충분 — "
                  << Clr::BrGreen << "출고 대기(CONFIRMED)" << Clr::Reset
                  << " 상태로 전환되었습니다.\n";
    } else {
        std::cout << "  → 재고 부족 — "
                  << Clr::BrYellow << "생산 중(PRODUCING)" << Clr::Reset
                  << " 상태로 생산 큐에 등록되었습니다.\n";
    }
}

void OrderView::showRejectResult(const Order& order) {
    MainMenuView::showSuccess("주문 거절 처리: " + order.order_number);
    std::cout << "  → " << Clr::BrRed << "거절됨(REJECTED)" << Clr::Reset << "\n";
    if (!order.note.empty())
        std::cout << "  사유: " << order.note << "\n";
}
