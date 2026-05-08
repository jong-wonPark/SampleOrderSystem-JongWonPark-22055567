#include "ProductionService.h"

ProductionService::ProductionService(
    ProductionQueueRepository& queueRepo,
    OrderRepository&           orderRepo,
    InventoryService&          inventoryService)
    : queueRepo_(queueRepo),
      orderRepo_(orderRepo),
      inventoryService_(inventoryService)
{}

ProductionQueueItem ProductionService::enqueueProduction(
    const std::string& order_number,
    const std::string& sample_id,
    const std::string& sample_name,
    int    planned_quantity,
    double total_production_time_hours)
{
    return queueRepo_.enqueue(order_number, sample_id, sample_name,
                              planned_quantity, total_production_time_hours);
}

bool ProductionService::startNextProduction() {
    auto front = queueRepo_.getFront();
    if (!front.has_value()) return false;
    try {
        queueRepo_.start(front->production_id);
        return true;
    } catch (...) { return false; }
}

bool ProductionService::completeProduction(const std::string& production_id) {
    auto item = queueRepo_.findById(production_id);
    if (!item.has_value()) return false;
    if (item->status != ProductionQueueStatus::InProduction) return false;

    try {
        auto completed = queueRepo_.dequeue(production_id);

        // 생산된 재고 입고 후 주문량 차감
        inventoryService_.addStock(completed.sample_id, completed.planned_quantity);
        auto order = orderRepo_.findByOrderNumber(completed.order_number);
        if (order.has_value()) {
            inventoryService_.deductStock(completed.sample_id, order->order_quantity);
            orderRepo_.updateStatus(completed.order_number, OrderStatus::CONFIRMED);
        }
        return true;
    } catch (...) { return false; }
}

bool ProductionService::shipOrder(const std::string& order_number) {
    auto order = orderRepo_.findByOrderNumber(order_number);
    if (!order.has_value()) return false;
    if (order->status != OrderStatus::CONFIRMED) return false;
    try {
        orderRepo_.updateStatus(order_number, OrderStatus::RELEASE);
        return true;
    } catch (...) { return false; }
}

std::vector<ProductionQueueItem> ProductionService::getQueue() const {
    return queueRepo_.findAll();
}

std::optional<ProductionQueueItem> ProductionService::getCurrentProduction() const {
    for (const auto& p : queueRepo_.findAll())
        if (p.status == ProductionQueueStatus::InProduction) return p;
    return std::nullopt;
}
