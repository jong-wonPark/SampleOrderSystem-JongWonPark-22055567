#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../Models/Enums.h"
#include "../Models/SampleItem.h"
#include "../Models/InventoryItem.h"
#include "../Models/Order.h"
#include "../Models/ProductionQueueItem.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// ── JSON 직렬화 선언 (ADL — Model 헤더가 nlohmann/json 의존 금지) ─

void to_json(json& j, const Order& o);
void from_json(const json& j, Order& o);

void to_json(json& j, const ProductionQueueItem& p);
void from_json(const json& j, ProductionQueueItem& p);


// ══════════════════════════════════════════════════════════════════
// InventoryManager — inventory.json
// SampleItem + InventoryItem 병합 레코드 관리
// ══════════════════════════════════════════════════════════════════

class InventoryManager {
public:
    explicit InventoryManager(fs::path file_path);

    // ── Create ────────────────────────────────────────────────────
    // 중복 sample_id 시 std::invalid_argument
    SampleItem add_sample(const std::string& sample_id,
                          const std::string& sample_name,
                          double avg_production_time_hours,
                          double yield_rate,
                          int    initial_quantity = 0,
                          const std::string& unit = "EA");

    // ── Read (SampleItem 관점) ────────────────────────────────────
    std::vector<SampleItem>   get_all_samples() const;
    std::optional<SampleItem> get_sample_by_id(const std::string& sample_id) const;
    std::vector<SampleItem>   find_samples_by_name(const std::string& keyword) const;

    // ── Read (InventoryItem 관점) ─────────────────────────────────
    std::vector<InventoryItem>   get_all_inventory() const;
    std::optional<InventoryItem> get_inventory_by_id(const std::string& sample_id) const;

    // ── Update ────────────────────────────────────────────────────
    // delta > 0: 입고 / delta < 0: 출고 (잔량 부족 시 std::runtime_error)
    InventoryItem update_stock(const std::string& sample_id, int delta);

    // ── Delete ────────────────────────────────────────────────────
    void remove_sample(const std::string& sample_id);

private:
    fs::path file_path_;
    json load() const;
    void save(const json& arr) const;
};


// ══════════════════════════════════════════════════════════════════
// OrderManager — orders.json
// ══════════════════════════════════════════════════════════════════

class OrderManager {
public:
    explicit OrderManager(fs::path file_path);

    // ── Create ────────────────────────────────────────────────────
    // order_number 자동 채번 (ORD-YYYYMMDD-XXXX), status = RESERVED
    Order add(const std::string& sample_id,
              const std::string& sample_name,
              const std::string& customer_name,
              int                order_quantity);

    // ── Read ──────────────────────────────────────────────────────
    std::vector<Order>   get_all() const;
    std::optional<Order> get_by_order_number(const std::string& order_number) const;
    std::vector<Order>   get_by_status(OrderStatus status) const;

    // ── Update ────────────────────────────────────────────────────
    // 존재하지 않는 order_number 시 std::out_of_range
    Order update_status(const std::string& order_number,
                        OrderStatus        new_status,
                        const std::string& note = "");

    // ── Delete ────────────────────────────────────────────────────
    void remove(const std::string& order_number);

private:
    fs::path file_path_;
    json load() const;
    void save(const json& arr) const;
    std::string new_order_number(const json& arr) const;
};


// ══════════════════════════════════════════════════════════════════
// ProductionQueueManager — production_queue.json
// 모든 조회 결과는 queued_at ASC 정렬 (FIFO 보장)
// ══════════════════════════════════════════════════════════════════

class ProductionQueueManager {
public:
    explicit ProductionQueueManager(fs::path file_path);

    // ── Create ────────────────────────────────────────────────────
    // production_id 자동 채번 (PROD-YYYYMMDD-XXXX), status = Waiting
    ProductionQueueItem enqueue(const std::string& order_number,
                                const std::string& sample_id,
                                const std::string& sample_name,
                                int                planned_quantity);

    // ── Read (queued_at ASC) ──────────────────────────────────────
    std::vector<ProductionQueueItem>   get_all() const;
    std::optional<ProductionQueueItem> get_by_id(const std::string& production_id) const;
    std::vector<ProductionQueueItem>   get_by_status(ProductionQueueStatus status) const;
    // Waiting 항목 중 queued_at 가장 이른 것
    std::optional<ProductionQueueItem> get_front() const;

    // ── Update ────────────────────────────────────────────────────
    // Waiting → InProduction (Waiting 아니면 std::runtime_error)
    ProductionQueueItem start(const std::string& production_id);
    // InProduction → 큐에서 제거 후 반환 (InProduction 아니면 std::runtime_error)
    ProductionQueueItem complete(const std::string& production_id);

    // ── Delete ────────────────────────────────────────────────────
    // Waiting 상태만 취소 가능 (그 외 std::runtime_error)
    void cancel(const std::string& production_id);

private:
    fs::path file_path_;
    json load() const;
    void save(const json& arr) const;
    std::string new_production_id(const json& arr) const;
};
