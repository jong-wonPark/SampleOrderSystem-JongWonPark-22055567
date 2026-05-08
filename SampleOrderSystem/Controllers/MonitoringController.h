#pragma once
#include "../Services/OrderService.h"
#include "../Services/InventoryService.h"

class MonitoringController {
public:
    MonitoringController(OrderService& ordSvc, InventoryService& invSvc);
    void run();   // 대시보드 출력 후 pause
private:
    OrderService&     ordSvc_;
    InventoryService& invSvc_;
};
