#pragma once
#include "../Services/ProductionService.h"
#include "../Services/OrderService.h"

class ProductionController {
public:
    ProductionController(ProductionService& prodSvc, OrderService& ordSvc);
    void runShipping();    // 메뉴 5: 단일 액션
    void runProduction();  // 메뉴 6: 현황 표시 (자동 시작·완료) + Enter 뒤로가기
private:
    ProductionService& prodSvc_;
    OrderService&      ordSvc_;
    void handleShip();
};
