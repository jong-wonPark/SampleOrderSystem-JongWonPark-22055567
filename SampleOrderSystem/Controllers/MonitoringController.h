#pragma once
#include "../Services/OrderService.h"
#include "../Services/InventoryService.h"
#include "../Services/ProductionService.h"

class MonitoringController {
public:
    MonitoringController(OrderService&      ordSvc,
                         InventoryService&  invSvc,
                         ProductionService& prodSvc);
    void run();   // 대시보드 출력 후 pause
private:
    OrderService&      ordSvc_;
    InventoryService&  invSvc_;
    ProductionService& prodSvc_;
};
