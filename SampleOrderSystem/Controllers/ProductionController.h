#pragma once
#include "../Services/ProductionService.h"
#include "../Services/OrderService.h"

class ProductionController {
public:
    ProductionController(ProductionService& prodSvc, OrderService& ordSvc);
    void runShipping();    // 메뉴 5: 단일 액션
    void runProduction();  // 메뉴 6: 서브메뉴 루프 (1:생산시작 2:생산완료 0:뒤로)
private:
    ProductionService& prodSvc_;
    OrderService&      ordSvc_;
    void handleShip();
    void handleStartProduction();
    void handleCompleteProduction();
};
