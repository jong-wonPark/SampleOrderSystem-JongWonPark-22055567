#include "ProductionService.h"

#include <cmath>
#include <cstdio>
#include <ctime>

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
        // 실제 시작 시점의 재고로 생산량 재계산 (FIFO 순서 보장)
        auto order = orderRepo_.findByOrderNumber(front->order_number);
        if (!order.has_value()) return false;

        int current_stock = 0;
        if (auto inv = inventoryService_.getStock(front->sample_id))
            current_stock = inv->quantity;

        int shortage = std::max(0, order->order_quantity - current_stock);

        double yield_rate = 1.0, avg_time_h = 0.0;
        if (auto sample = inventoryService_.findSampleById(front->sample_id)) {
            yield_rate = sample->yield_rate;
            avg_time_h = sample->avg_production_time_hours;
        }
        int    planned_qty = (shortage > 0)
            ? static_cast<int>(std::ceil(shortage / (yield_rate * 0.9))) : 0;
        double total_time  = planned_qty * avg_time_h;

        queueRepo_.start_with_quantities(front->production_id, planned_qty, total_time);
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

std::vector<std::string> ProductionService::autoCompleteExpired() {
    // 스냅샷으로 순회 (completeProduction이 큐를 수정하므로)
    auto snapshot = queueRepo_.findAll();
    auto now      = std::time(nullptr);

    std::vector<std::string> completedOrderNums;
    for (const auto& p : snapshot) {
        if (p.status != ProductionQueueStatus::InProduction) continue;
        if (p.estimated_completion.empty()) continue;

        int y, mo, d, h, mi, s;
        if (sscanf_s(p.estimated_completion.c_str(), "%d-%d-%dT%d:%d:%d",
                     &y, &mo, &d, &h, &mi, &s) != 6) continue;
        std::tm tm{};
        tm.tm_year = y-1900; tm.tm_mon = mo-1; tm.tm_mday = d;
        tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = s;
        tm.tm_isdst = -1;

        if (now >= std::mktime(&tm)) {
            std::string orderNum = p.order_number;
            if (completeProduction(p.production_id))
                completedOrderNums.push_back(orderNum);
        }
    }
    return completedOrderNums;
}
