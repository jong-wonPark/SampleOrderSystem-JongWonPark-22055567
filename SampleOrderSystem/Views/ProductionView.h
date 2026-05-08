#pragma once
#include <string>
#include <vector>

#include "../Models/Order.h"
#include "../Models/ProductionQueueItem.h"
#include "MainMenuView.h"

class ProductionView {
public:
    // 메뉴 5: 출고 처리
    static void        showConfirmedOrders(const std::vector<Order>& orders);
    static std::string promptShipOrderNumber();
    static void showShipResult(const Order& order);

    // 메뉴 6: 생산 라인
    static int  promptProductionSubMenu();  // 1:생산시작 2:생산완료 0:뒤로
    static void showProductionQueue(const std::vector<ProductionQueueItem>& queue);
    static void showStartResult(const ProductionQueueItem& item);
    static std::string promptCompleteProductionId();
    static void showCompleteResult(const Order& confirmedOrder);
};
