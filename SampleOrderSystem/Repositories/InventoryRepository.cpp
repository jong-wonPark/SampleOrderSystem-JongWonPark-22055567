#include "InventoryRepository.h"

#include <algorithm>
#include <cctype>

InventoryRepository::InventoryRepository(InventoryManager& mgr)
    : mgr_(mgr),
      samples_(mgr.get_all_samples()),
      inventory_(mgr.get_all_inventory())
{}

SampleItem InventoryRepository::addSample(
    const std::string& sample_id,
    const std::string& sample_name,
    double avg_production_time_hours,
    double yield_rate,
    int    initial_quantity,
    const std::string& unit)
{
    SampleItem s = mgr_.add_sample(sample_id, sample_name,
                                   avg_production_time_hours, yield_rate,
                                   initial_quantity, unit);
    samples_.push_back(s);
    // Manager가 타임스탬프를 채운 InventoryItem을 가져와 캐시에 추가
    if (auto inv = mgr_.get_inventory_by_id(sample_id))
        inventory_.push_back(*inv);
    return s;
}

std::vector<SampleItem> InventoryRepository::findAllSamples() const {
    return samples_;
}

std::optional<SampleItem> InventoryRepository::findSampleById(
    const std::string& sample_id) const
{
    for (const auto& s : samples_)
        if (s.sample_id == sample_id) return s;
    return std::nullopt;
}

std::vector<SampleItem> InventoryRepository::findSamplesByName(
    const std::string& keyword) const
{
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return s;
    };
    const std::string lkw = lower(keyword);

    std::vector<SampleItem> result;
    for (const auto& s : samples_)
        if (lower(s.sample_name).find(lkw) != std::string::npos)
            result.push_back(s);
    return result;
}

std::vector<InventoryItem> InventoryRepository::findAllInventory() const {
    return inventory_;
}

std::optional<InventoryItem> InventoryRepository::findInventoryById(
    const std::string& sample_id) const
{
    for (const auto& inv : inventory_)
        if (inv.sample_id == sample_id) return inv;
    return std::nullopt;
}

InventoryItem InventoryRepository::updateStock(
    const std::string& sample_id, int delta)
{
    InventoryItem updated = mgr_.update_stock(sample_id, delta);
    for (auto& inv : inventory_)
        if (inv.sample_id == sample_id) { inv = updated; break; }
    return updated;
}

bool InventoryRepository::hasSufficientStock(
    const std::string& sample_id, int quantity) const
{
    auto inv = findInventoryById(sample_id);
    return inv.has_value() && inv->quantity >= quantity;
}

void InventoryRepository::removeSample(const std::string& sample_id) {
    mgr_.remove_sample(sample_id);
    auto rm_if = [&](auto& vec, auto pred) {
        vec.erase(std::remove_if(vec.begin(), vec.end(), pred), vec.end());
    };
    rm_if(samples_,   [&](const SampleItem& s)    { return s.sample_id    == sample_id; });
    rm_if(inventory_, [&](const InventoryItem& inv){ return inv.sample_id  == sample_id; });
}
