#pragma once
#include "../Services/OrderService.h"
#include "../Services/InventoryService.h"

class OrderController {
public:
    OrderController(OrderService& ordSvc, InventoryService& invSvc);
    void runPlaceOrder();  // 메뉴 2: 단일 액션
    void runApproval();    // 메뉴 3: 서브메뉴 루프 (1:승인 2:거절 0:뒤로)
private:
    OrderService&     ordSvc_;
    InventoryService& invSvc_;
    void handlePlaceOrder();
};
