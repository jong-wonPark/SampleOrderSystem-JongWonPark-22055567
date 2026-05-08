#include "InventoryService.h"

InventoryService::InventoryService(InventoryRepository& repo)
    : repo_(repo)
{}

SampleItem InventoryService::registerSample(
    const std::string& sample_id,
    const std::string& sample_name,
    double avg_production_time_hours,
    double yield_rate,
    int initial_quantity)
{
    return repo_.addSample(sample_id, sample_name,
                           avg_production_time_hours, yield_rate,
                           initial_quantity);
}

std::vector<SampleItem> InventoryService::getAllSamples() const {
    return repo_.findAllSamples();
}

std::optional<SampleItem> InventoryService::findSampleById(
    const std::string& sample_id) const
{
    return repo_.findSampleById(sample_id);
}

std::vector<SampleItem> InventoryService::findSamplesByName(
    const std::string& keyword) const
{
    return repo_.findSamplesByName(keyword);
}

std::optional<InventoryItem> InventoryService::getStock(
    const std::string& sample_id) const
{
    return repo_.findInventoryById(sample_id);
}

std::vector<InventoryItem> InventoryService::getAllInventory() const {
    return repo_.findAllInventory();
}

bool InventoryService::hasEnoughStock(
    const std::string& sample_id, int quantity) const
{
    return repo_.hasSufficientStock(sample_id, quantity);
}

bool InventoryService::deductStock(const std::string& sample_id, int quantity) {
    try {
        repo_.updateStock(sample_id, -quantity);
        return true;
    } catch (...) { return false; }
}

bool InventoryService::addStock(const std::string& sample_id, int quantity) {
    try {
        repo_.updateStock(sample_id, +quantity);
        return true;
    } catch (...) { return false; }
}
