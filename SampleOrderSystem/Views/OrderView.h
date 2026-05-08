#pragma once
#include <string>
#include <vector>

#include "../Models/Order.h"
#include "MainMenuView.h"

struct OrderInput {
    std::string sample_id;
    std::string customer_name;
    int         order_quantity;
};

class OrderView {
public:
    // 메뉴 2: 주문 접수
    static OrderInput promptOrderInput();
    static void showPlaceOrderResult(const Order& order);

    // 메뉴 3: 승인/거절
    static void showReservedOrders(const std::vector<Order>& orders);
    // 목록에서 번호로 선택 (1~count, 0=뒤로)
    static int  promptSelectOrder(int count);
    // 선택된 주문 요약 표시
    static void showSelectedOrder(const Order& order);
    // 1.승인 / 2.거절 / 0.뒤로
    static int  promptApproveOrReject();
    static std::string promptRejectNote();
    static void showApproveResult(const Order& order);
    static void showRejectResult(const Order& order);
};
