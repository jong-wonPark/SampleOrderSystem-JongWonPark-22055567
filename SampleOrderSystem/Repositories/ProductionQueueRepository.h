#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../Models/ProductionQueueItem.h"
#include "../Models/Enums.h"
#include "../Persistence/DataPersistence.h"

class ProductionQueueRepository {
public:
    explicit ProductionQueueRepository(ProductionQueueManager& mgr);

    // ── Create ────────────────────────────────────────────────────
    ProductionQueueItem enqueue(const std::string& order_number,
                                const std::string& sample_id,
                                const std::string& sample_name,
                                int                planned_quantity);

    // ── Read (queued_at ASC) ──────────────────────────────────────
    std::vector<ProductionQueueItem>   findAll() const;
    std::optional<ProductionQueueItem> findById(const std::string& production_id) const;
    // Waiting 항목만, FIFO 순서
    std::vector<ProductionQueueItem>   getWaitingQueue() const;
    // Waiting 항목 중 queued_at 가장 이른 것
    std::optional<ProductionQueueItem> getFront() const;

    // ── Update ────────────────────────────────────────────────────
    // Waiting → InProduction
    ProductionQueueItem start(const std::string& production_id);

    // ── Delete + Return ───────────────────────────────────────────
    // 생산 완료: 큐에서 제거 후 항목 반환
    ProductionQueueItem dequeue(const std::string& production_id);

    // Waiting 항목만 취소
    void cancel(const std::string& production_id);

private:
    ProductionQueueManager&          mgr_;
    std::vector<ProductionQueueItem> queue_;  // 인메모리 캐시, queued_at ASC 유지
};
