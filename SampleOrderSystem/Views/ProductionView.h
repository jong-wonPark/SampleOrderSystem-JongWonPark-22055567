#pragma once
#include <string>
#include <vector>

#include "../Models/Order.h"
#include "../Models/ProductionQueueItem.h"
#include "MainMenuView.h"

class ProductionView {
public:
    // 메뉴 5: 출고 처리
    static void showConfirmedOrders(const std::vector<Order>& orders);
    static int  promptSelectShipOrder(int count);   // 1~count, 0=취소
    static void showShipResult(const Order& order);

    // 메뉴 6: 생산 라인
    static int  promptProductionSubMenu();  // 1:생산시작 2:생산완료 0:뒤로
    static void showProductionQueue(const std::vector<ProductionQueueItem>& queue);
    static int  promptSelectInProduction(int count);  // 1~count, 0=취소
    static void showStartResult(const ProductionQueueItem& item);
    static void showCompleteResult(const Order& confirmedOrder);
};
