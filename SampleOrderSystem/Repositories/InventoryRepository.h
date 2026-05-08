#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../Models/SampleItem.h"
#include "../Models/InventoryItem.h"
#include "../Persistence/DataPersistence.h"

class InventoryRepository {
public:
    explicit InventoryRepository(InventoryManager& mgr);

    // ── SampleItem 관련 (메뉴 1: 시료 관리) ──────────────────────
    SampleItem              addSample(const std::string& sample_id,
                                      const std::string& sample_name,
                                      double avg_production_time_hours,
                                      double yield_rate,
                                      int    initial_quantity = 0,
                                      const std::string& unit = "EA");
    std::vector<SampleItem>   findAllSamples() const;
    std::optional<SampleItem> findSampleById(const std::string& sample_id) const;
    std::vector<SampleItem>   findSamplesByName(const std::string& keyword) const;
    void removeSample(const std::string& sample_id);

    // ── InventoryItem 관련 (재고 확인) ───────────────────────────
    std::vector<InventoryItem>   findAllInventory() const;
    std::optional<InventoryItem> findInventoryById(const std::string& sample_id) const;
    // delta > 0: 입고 / delta < 0: 출고
    InventoryItem updateStock(const std::string& sample_id, int delta);
    bool hasSufficientStock(const std::string& sample_id, int quantity) const;

private:
    InventoryManager&          mgr_;
    std::vector<SampleItem>    samples_;    // 인메모리 캐시
    std::vector<InventoryItem> inventory_;  // 인메모리 캐시
};
