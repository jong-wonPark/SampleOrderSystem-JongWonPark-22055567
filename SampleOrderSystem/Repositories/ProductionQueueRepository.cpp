#include "ProductionQueueRepository.h"

#include <algorithm>

ProductionQueueRepository::ProductionQueueRepository(ProductionQueueManager& mgr)
    : mgr_(mgr),
      queue_(mgr.get_all())  // Manager가 이미 queued_at ASC 정렬하여 반환
{}

ProductionQueueItem ProductionQueueRepository::enqueue(
    const std::string& order_number,
    const std::string& sample_id,
    const std::string& sample_name,
    int    planned_quantity,
    double total_production_time_hours)
{
    ProductionQueueItem p = mgr_.enqueue(order_number, sample_id, sample_name,
                                         planned_quantity, total_production_time_hours);
    queue_.push_back(p);  // 최신 항목은 항상 말단 → FIFO 유지
    return p;
}

std::vector<ProductionQueueItem> ProductionQueueRepository::findAll() const {
    return queue_;
}

std::optional<ProductionQueueItem> ProductionQueueRepository::findById(
    const std::string& production_id) const
{
    for (const auto& p : queue_)
        if (p.production_id == production_id) return p;
    return std::nullopt;
}

std::vector<ProductionQueueItem> ProductionQueueRepository::getWaitingQueue() const {
    std::vector<ProductionQueueItem> result;
    for (const auto& p : queue_)
        if (p.status == ProductionQueueStatus::Waiting)
            result.push_back(p);
    return result;
}

std::optional<ProductionQueueItem> ProductionQueueRepository::getFront() const {
    for (const auto& p : queue_)
        if (p.status == ProductionQueueStatus::Waiting) return p;
    return std::nullopt;
}

ProductionQueueItem ProductionQueueRepository::start(
    const std::string& production_id)
{
    ProductionQueueItem updated = mgr_.start(production_id);
    for (auto& p : queue_)
        if (p.production_id == production_id) { p = updated; break; }
    return updated;
}

ProductionQueueItem ProductionQueueRepository::dequeue(
    const std::string& production_id)
{
    ProductionQueueItem completed = mgr_.complete(production_id);
    queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(),
            [&](const ProductionQueueItem& p) {
                return p.production_id == production_id;
            }),
        queue_.end());
    return completed;
}

void ProductionQueueRepository::cancel(const std::string& production_id) {
    mgr_.cancel(production_id);
    queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(),
            [&](const ProductionQueueItem& p) {
                return p.production_id == production_id;
            }),
        queue_.end());
}
