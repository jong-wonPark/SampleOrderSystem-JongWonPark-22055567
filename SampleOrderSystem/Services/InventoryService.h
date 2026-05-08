#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../Models/SampleItem.h"
#include "../Models/InventoryItem.h"
#include "../Repositories/InventoryRepository.h"

class InventoryService {
public:
    explicit InventoryService(InventoryRepository& repo);

    // ── 시료 마스터 관리 (메뉴 1) ────────────────────────────────
    SampleItem              registerSample(const std::string& sample_id,
                                           const std::string& sample_name,
                                           double avg_production_time_hours,
                                           double yield_rate,
                                           int    initial_quantity = 0);
    std::vector<SampleItem>   getAllSamples() const;
    std::optional<SampleItem> findSampleById(const std::string& sample_id) const;
    std::vector<SampleItem>   findSamplesByName(const std::string& keyword) const;

    // ── 재고 관리 ─────────────────────────────────────────────────
    std::optional<InventoryItem> getStock(const std::string& sample_id) const;
    std::vector<InventoryItem>   getAllInventory() const;
    bool hasEnoughStock(const std::string& sample_id, int quantity) const;
    // 출고: 재고 부족 시 false 반환 (예외 전파 없음)
    bool deductStock(const std::string& sample_id, int quantity);
    // 입고: 생산 완료 시 호출
    bool addStock(const std::string& sample_id, int quantity);

private:
    InventoryRepository& repo_;
};
