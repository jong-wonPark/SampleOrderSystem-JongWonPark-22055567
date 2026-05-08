#pragma once
#include <vector>

#include "../Models/Order.h"
#include "../Models/SampleItem.h"
#include "../Models/InventoryItem.h"
#include "../Models/ProductionQueueItem.h"
#include "MainMenuView.h"

class MonitoringView {
public:
    static void showDashboard(const std::vector<Order>&               orders,
                              const std::vector<SampleItem>&          samples,
                              const std::vector<InventoryItem>&       inventory,
                              const std::vector<ProductionQueueItem>& queue);
private:
    static void showOrderSummary(const std::vector<Order>& orders);
    static void showInventoryTable(const std::vector<SampleItem>&          samples,
                                   const std::vector<InventoryItem>&       inventory,
                                   const std::vector<Order>&               orders,
                                   const std::vector<ProductionQueueItem>& queue);
};
