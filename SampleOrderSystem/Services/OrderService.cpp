#include "OrderService.h"
#include <cmath>

OrderService::OrderService(
    OrderRepository&   orderRepo,
    InventoryService&  inventoryService,
    ProductionService& productionService)
    : orderRepo_(orderRepo),
      inventoryService_(inventoryService),
      productionService_(productionService)
{}

Order OrderService::placeOrder(
    const std::string& sample_id,
    const std::string& sample_name,
    const std::string& customer_name,
    int order_quantity)
{
    return orderRepo_.add(sample_id, sample_name, customer_name, order_quantity);
}

bool OrderService::approveOrder(const std::string& order_number) {
    auto order = orderRepo_.findByOrderNumber(order_number);
    if (!order.has_value()) return false;
    if (order->status != OrderStatus::RESERVED) return false;

    if (inventoryService_.hasEnoughStock(order->sample_id, order->order_quantity)) {
        inventoryService_.deductStock(order->sample_id, order->order_quantity);
        orderRepo_.updateStatus(order_number, OrderStatus::CONFIRMED);
    } else {
        // 부족분 계산
        int current_stock = 0;
        if (auto inv = inventoryService_.getStock(order->sample_id))
            current_stock = inv->quantity;
        int shortage = order->order_quantity - current_stock;

        // 생산량 = ceil(부족분 / (수율 × 0.9))
        double yield_rate = 1.0;
        double avg_time_h = 0.0;
        if (auto sample = inventoryService_.findSampleById(order->sample_id)) {
            yield_rate = sample->yield_rate;
            avg_time_h = sample->avg_production_time_hours;
        }
        int planned_qty = static_cast<int>(
            std::ceil(shortage / (yield_rate * 0.9)));

        // 총 생산 시간 = 생산량 × 평균 생산시간
        double total_time_h = planned_qty * avg_time_h;

        productionService_.enqueueProduction(
            order_number,
            order->sample_id,
            order->sample_name,
            planned_qty,
            total_time_h);
        orderRepo_.updateStatus(order_number, OrderStatus::PRODUCING);
    }
    return true;
}

bool OrderService::rejectOrder(
    const std::string& order_number, const std::string& note)
{
    auto order = orderRepo_.findByOrderNumber(order_number);
    if (!order.has_value()) return false;
    if (order->status != OrderStatus::RESERVED) return false;
    try {
        orderRepo_.updateStatus(order_number, OrderStatus::REJECTED, note);
        return true;
    } catch (...) { return false; }
}

std::vector<Order> OrderService::getAllOrders() const {
    return orderRepo_.findAll();
}

std::vector<Order> OrderService::getOrdersByStatus(OrderStatus status) const {
    return orderRepo_.findByStatus(status);
}

std::optional<Order> OrderService::findByOrderNumber(
    const std::string& order_number) const
{
    return orderRepo_.findByOrderNumber(order_number);
}
