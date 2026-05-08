#include "OrderRepository.h"

#include <algorithm>

OrderRepository::OrderRepository(OrderManager& mgr)
    : mgr_(mgr), orders_(mgr.get_all())
{}

Order OrderRepository::add(
    const std::string& sample_id,
    const std::string& sample_name,
    const std::string& customer_name,
    int order_quantity)
{
    Order o = mgr_.add(sample_id, sample_name, customer_name, order_quantity);
    orders_.push_back(o);
    return o;
}

std::vector<Order> OrderRepository::findAll() const {
    return orders_;
}

std::optional<Order> OrderRepository::findByOrderNumber(
    const std::string& order_number) const
{
    for (const auto& o : orders_)
        if (o.order_number == order_number) return o;
    return std::nullopt;
}

std::vector<Order> OrderRepository::findByStatus(OrderStatus status) const {
    std::vector<Order> result;
    for (const auto& o : orders_)
        if (o.status == status) result.push_back(o);
    return result;
}

Order OrderRepository::updateStatus(
    const std::string& order_number,
    OrderStatus        new_status,
    const std::string& note)
{
    Order updated = mgr_.update_status(order_number, new_status, note);
    for (auto& o : orders_)
        if (o.order_number == order_number) { o = updated; break; }
    return updated;
}

void OrderRepository::remove(const std::string& order_number) {
    mgr_.remove(order_number);
    orders_.erase(
        std::remove_if(orders_.begin(), orders_.end(),
            [&](const Order& o) { return o.order_number == order_number; }),
        orders_.end());
}
