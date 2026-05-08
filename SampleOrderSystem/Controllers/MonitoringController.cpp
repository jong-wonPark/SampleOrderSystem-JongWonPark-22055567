#include "MonitoringController.h"
#include "../Views/MonitoringView.h"
#include "../Views/MainMenuView.h"

MonitoringController::MonitoringController(OrderService&      ordSvc,
                                           InventoryService&  invSvc,
                                           ProductionService& prodSvc)
    : ordSvc_(ordSvc), invSvc_(invSvc), prodSvc_(prodSvc)
{}

void MonitoringController::run() {
    MonitoringView::showDashboard(
        ordSvc_.getAllOrders(),
        invSvc_.getAllSamples(),
        invSvc_.getAllInventory(),
        prodSvc_.getQueue());           // 생산 큐 포함
    MainMenuView::pause();
}
