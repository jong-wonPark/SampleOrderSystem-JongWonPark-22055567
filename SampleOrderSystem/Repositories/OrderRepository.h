#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../Models/Order.h"
#include "../Models/Enums.h"
#include "../Persistence/DataPersistence.h"

class OrderRepository {
public:
    explicit OrderRepository(OrderManager& mgr);

    // ── Create ────────────────────────────────────────────────────
    Order add(const std::string& sample_id,
              const std::string& sample_name,
              const std::string& customer_name,
              int                order_quantity);

    // ── Read ──────────────────────────────────────────────────────
    std::vector<Order>   findAll() const;
    std::optional<Order> findByOrderNumber(const std::string& order_number) const;
    std::vector<Order>   findByStatus(OrderStatus status) const;

    // ── Update ────────────────────────────────────────────────────
    Order updateStatus(const std::string& order_number,
                       OrderStatus        new_status,
                       const std::string& note = "");

    // ── Delete ────────────────────────────────────────────────────
    void remove(const std::string& order_number);

private:
    OrderManager&       mgr_;
    std::vector<Order>  orders_;  // 인메모리 캐시
};
