#include "MonitoringController.h"
#include "../Views/MonitoringView.h"
#include "../Views/MainMenuView.h"

MonitoringController::MonitoringController(OrderService& ordSvc, InventoryService& invSvc)
    : ordSvc_(ordSvc), invSvc_(invSvc)
{}

void MonitoringController::run() {
    MonitoringView::showDashboard(
        ordSvc_.getAllOrders(),
        invSvc_.getAllSamples(),
        invSvc_.getAllInventory());
    MainMenuView::pause();
}
