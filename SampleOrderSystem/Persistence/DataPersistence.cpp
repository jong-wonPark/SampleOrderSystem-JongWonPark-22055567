#include "DataPersistence.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// ── 내부 유틸리티 ──────────────────────────────────────────────────

static std::string now_iso8601() {
    auto t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

static std::string today_str() {
    auto t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d");
    return oss.str();
}

static json load_array(const fs::path& path) {
    if (!fs::exists(path)) return json::array();
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open file: " + path.string());
    try {
        json j;
        f >> j;
        return j.is_array() ? j : json::array();
    }
    catch (const json::parse_error& e) {
        throw std::runtime_error("JSON parse error in " + path.string() + ": " + e.what());
    }
}

static void save_array(const fs::path& path, const json& arr) {
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open())
        throw std::runtime_error("Cannot save file: " + path.string());
    f << std::setw(2) << arr;
}

// ── JSON 직렬화 구현 ──────────────────────────────────────────────

void to_json(json& j, const Order& o) {
    j = {
        {"order_number",   o.order_number},
        {"sample_id",      o.sample_id},
        {"sample_name",    o.sample_name},
        {"customer_name",  o.customer_name},
        {"order_quantity", o.order_quantity},
        {"status",         toJsonString(o.status)},
        {"order_date",     o.order_date},
        {"note",           o.note}
    };
}

void from_json(const json& j, Order& o) {
    j.at("order_number").get_to(o.order_number);
    j.at("sample_id").get_to(o.sample_id);
    j.at("sample_name").get_to(o.sample_name);
    j.at("customer_name").get_to(o.customer_name);
    j.at("order_quantity").get_to(o.order_quantity);
    o.status = orderStatusFromJson(j.at("status").get<std::string>());
    j.at("order_date").get_to(o.order_date);
    j.at("note").get_to(o.note);
}

void to_json(json& j, const ProductionQueueItem& p) {
    j = {
        {"production_id",              p.production_id},
        {"order_number",               p.order_number},
        {"sample_id",                  p.sample_id},
        {"sample_name",                p.sample_name},
        {"planned_quantity",           p.planned_quantity},
        {"status",                     toJsonString(p.status)},
        {"queued_at",                  p.queued_at},
        {"started_at",                 p.started_at},
        {"total_production_time_hours", p.total_production_time_hours},
        {"estimated_completion",        p.estimated_completion}
    };
}

void from_json(const json& j, ProductionQueueItem& p) {
    j.at("production_id").get_to(p.production_id);
    j.at("order_number").get_to(p.order_number);
    j.at("sample_id").get_to(p.sample_id);
    j.at("sample_name").get_to(p.sample_name);
    j.at("planned_quantity").get_to(p.planned_quantity);
    p.status = productionQueueStatusFromJson(j.at("status").get<std::string>());
    j.at("queued_at").get_to(p.queued_at);
    j.at("started_at").get_to(p.started_at);
    // 기존 JSON 파일 호환성: 필드 없으면 기본값 유지
    if (j.contains("total_production_time_hours"))
        j.at("total_production_time_hours").get_to(p.total_production_time_hours);
    if (j.contains("estimated_completion"))
        j.at("estimated_completion").get_to(p.estimated_completion);
}


// ══════════════════════════════════════════════════════════════════
// InventoryManager
// ══════════════════════════════════════════════════════════════════

InventoryManager::InventoryManager(fs::path file_path)
    : file_path_(std::move(file_path)) {}

json InventoryManager::load() const { return load_array(file_path_); }
void InventoryManager::save(const json& arr) const { save_array(file_path_, arr); }

SampleItem InventoryManager::add_sample(
    const std::string& sample_id,
    const std::string& sample_name,
    double avg_production_time_hours,
    double yield_rate,
    int    initial_quantity,
    const std::string& unit)
{
    if (get_sample_by_id(sample_id).has_value())
        throw std::invalid_argument("Duplicate sample_id: " + sample_id);

    json arr = load();
    arr.push_back({
        {"sample_id",                 sample_id},
        {"sample_name",               sample_name},
        {"avg_production_time_hours", avg_production_time_hours},
        {"yield_rate",                yield_rate},
        {"quantity",                  initial_quantity},
        {"unit",                      unit},
        {"last_updated",              now_iso8601()}
    });
    save(arr);

    return {sample_id, sample_name, avg_production_time_hours, yield_rate};
}

std::vector<SampleItem> InventoryManager::get_all_samples() const {
    std::vector<SampleItem> result;
    for (const auto& rec : load()) {
        result.push_back({
            rec.at("sample_id").get<std::string>(),
            rec.at("sample_name").get<std::string>(),
            rec.at("avg_production_time_hours").get<double>(),
            rec.at("yield_rate").get<double>()
        });
    }
    return result;
}

std::optional<SampleItem> InventoryManager::get_sample_by_id(const std::string& sample_id) const {
    for (const auto& s : get_all_samples())
        if (s.sample_id == sample_id) return s;
    return std::nullopt;
}

std::vector<SampleItem> InventoryManager::find_samples_by_name(const std::string& keyword) const {
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return s;
    };
    const std::string lkw = lower(keyword);

    std::vector<SampleItem> result;
    for (const auto& rec : load()) {
        if (lower(rec.at("sample_name").get<std::string>()).find(lkw) != std::string::npos)
            result.push_back({
                rec.at("sample_id").get<std::string>(),
                rec.at("sample_name").get<std::string>(),
                rec.at("avg_production_time_hours").get<double>(),
                rec.at("yield_rate").get<double>()
            });
    }
    return result;
}

std::vector<InventoryItem> InventoryManager::get_all_inventory() const {
    std::vector<InventoryItem> result;
    for (const auto& rec : load())
        result.push_back({
            rec.at("sample_id").get<std::string>(),
            rec.at("quantity").get<int>(),
            rec.at("unit").get<std::string>(),
            rec.at("last_updated").get<std::string>()
        });
    return result;
}

std::optional<InventoryItem> InventoryManager::get_inventory_by_id(const std::string& sample_id) const {
    for (const auto& inv : get_all_inventory())
        if (inv.sample_id == sample_id) return inv;
    return std::nullopt;
}

InventoryItem InventoryManager::update_stock(const std::string& sample_id, int delta) {
    json arr = load();
    for (auto& rec : arr) {
        if (rec.at("sample_id").get<std::string>() != sample_id) continue;

        int cur  = rec.at("quantity").get<int>();
        int next = cur + delta;
        if (next < 0)
            throw std::runtime_error(
                "Insufficient stock for " + sample_id +
                ": current=" + std::to_string(cur) +
                ", requested=" + std::to_string(-delta));

        rec["quantity"]     = next;
        rec["last_updated"] = now_iso8601();
        save(arr);
        return {sample_id, next,
                rec.at("unit").get<std::string>(),
                rec.at("last_updated").get<std::string>()};
    }
    throw std::out_of_range("Unknown sample_id: " + sample_id);
}

void InventoryManager::remove_sample(const std::string& sample_id) {
    json arr = load();
    auto it  = std::remove_if(arr.begin(), arr.end(), [&](const json& r) {
        return r.at("sample_id").get<std::string>() == sample_id;
    });
    if (it == arr.end())
        throw std::out_of_range("Unknown sample_id: " + sample_id);
    arr.erase(it, arr.end());
    save(arr);
}


// ══════════════════════════════════════════════════════════════════
// OrderManager
// ══════════════════════════════════════════════════════════════════

OrderManager::OrderManager(fs::path file_path)
    : file_path_(std::move(file_path)) {}

json OrderManager::load() const { return load_array(file_path_); }
void OrderManager::save(const json& arr) const { save_array(file_path_, arr); }

std::string OrderManager::new_order_number(const json& arr) const {
    const std::string prefix = "ORD-" + today_str() + "-";
    int max_seq = 0;
    for (const auto& o : arr) {
        const std::string num = o.at("order_number").get<std::string>();
        if (num.rfind(prefix, 0) == 0) {
            try { max_seq = std::max(max_seq, std::stoi(num.substr(prefix.size()))); }
            catch (...) {}
        }
    }
    std::ostringstream oss;
    oss << prefix << std::setw(4) << std::setfill('0') << (max_seq + 1);
    return oss.str();
}

Order OrderManager::add(
    const std::string& sample_id,
    const std::string& sample_name,
    const std::string& customer_name,
    int order_quantity)
{
    json arr = load();
    Order o;
    o.order_number   = new_order_number(arr);
    o.sample_id      = sample_id;
    o.sample_name    = sample_name;
    o.customer_name  = customer_name;
    o.order_quantity = order_quantity;
    o.status         = OrderStatus::RESERVED;
    o.order_date     = now_iso8601();

    json j;
    to_json(j, o);
    arr.push_back(j);
    save(arr);
    return o;
}

std::vector<Order> OrderManager::get_all() const {
    return load().get<std::vector<Order>>();
}

std::optional<Order> OrderManager::get_by_order_number(const std::string& order_number) const {
    for (const auto& o : get_all())
        if (o.order_number == order_number) return o;
    return std::nullopt;
}

std::vector<Order> OrderManager::get_by_status(OrderStatus status) const {
    std::vector<Order> result;
    for (const auto& o : get_all())
        if (o.status == status) result.push_back(o);
    return result;
}

Order OrderManager::update_status(
    const std::string& order_number,
    OrderStatus        new_status,
    const std::string& note)
{
    json arr = load();
    for (auto& j_order : arr) {
        if (j_order.at("order_number").get<std::string>() != order_number) continue;
        j_order["status"] = toJsonString(new_status);
        if (!note.empty()) j_order["note"] = note;
        save(arr);
        Order o;
        from_json(j_order, o);
        return o;
    }
    throw std::out_of_range("Unknown order_number: " + order_number);
}

void OrderManager::remove(const std::string& order_number) {
    json arr = load();
    auto it  = std::remove_if(arr.begin(), arr.end(), [&](const json& j) {
        return j.at("order_number").get<std::string>() == order_number;
    });
    if (it == arr.end())
        throw std::out_of_range("Unknown order_number: " + order_number);
    arr.erase(it, arr.end());
    save(arr);
}


// ══════════════════════════════════════════════════════════════════
// ProductionQueueManager
// ══════════════════════════════════════════════════════════════════

ProductionQueueManager::ProductionQueueManager(fs::path file_path)
    : file_path_(std::move(file_path)) {}

json ProductionQueueManager::load() const { return load_array(file_path_); }
void ProductionQueueManager::save(const json& arr) const { save_array(file_path_, arr); }

std::string ProductionQueueManager::new_production_id(const json& arr) const {
    const std::string prefix = "PROD-" + today_str() + "-";
    int max_seq = 0;
    for (const auto& p : arr) {
        const std::string id = p.at("production_id").get<std::string>();
        if (id.rfind(prefix, 0) == 0) {
            try { max_seq = std::max(max_seq, std::stoi(id.substr(prefix.size()))); }
            catch (...) {}
        }
    }
    std::ostringstream oss;
    oss << prefix << std::setw(4) << std::setfill('0') << (max_seq + 1);
    return oss.str();
}

ProductionQueueItem ProductionQueueManager::enqueue(
    const std::string& order_number,
    const std::string& sample_id,
    const std::string& sample_name,
    int    planned_quantity,
    double total_production_time_hours)
{
    json arr = load();
    ProductionQueueItem p;
    p.production_id              = new_production_id(arr);
    p.order_number               = order_number;
    p.sample_id                  = sample_id;
    p.sample_name                = sample_name;
    p.planned_quantity           = planned_quantity;
    p.status                     = ProductionQueueStatus::Waiting;
    p.queued_at                  = now_iso8601();
    p.started_at                 = "";
    p.total_production_time_hours = total_production_time_hours;
    p.estimated_completion       = "";

    json j;
    to_json(j, p);
    arr.push_back(j);
    save(arr);
    return p;
}

std::vector<ProductionQueueItem> ProductionQueueManager::get_all() const {
    auto items = load().get<std::vector<ProductionQueueItem>>();
    std::stable_sort(items.begin(), items.end(),
        [](const ProductionQueueItem& a, const ProductionQueueItem& b) {
            return a.queued_at < b.queued_at;
        });
    return items;
}

std::optional<ProductionQueueItem> ProductionQueueManager::get_by_id(
    const std::string& production_id) const
{
    for (const auto& p : get_all())
        if (p.production_id == production_id) return p;
    return std::nullopt;
}

std::vector<ProductionQueueItem> ProductionQueueManager::get_by_status(
    ProductionQueueStatus status) const
{
    std::vector<ProductionQueueItem> result;
    for (const auto& p : get_all())
        if (p.status == status) result.push_back(p);
    return result;
}

std::optional<ProductionQueueItem> ProductionQueueManager::get_front() const {
    for (const auto& p : get_all())
        if (p.status == ProductionQueueStatus::Waiting) return p;
    return std::nullopt;
}

ProductionQueueItem ProductionQueueManager::start(const std::string& production_id) {
    json arr = load();
    for (auto& j_item : arr) {
        if (j_item.at("production_id").get<std::string>() != production_id) continue;
        if (productionQueueStatusFromJson(j_item.at("status").get<std::string>())
                != ProductionQueueStatus::Waiting)
            throw std::runtime_error(
                "Cannot start: not Waiting. production_id=" + production_id);
        j_item["status"]     = toJsonString(ProductionQueueStatus::InProduction);
        j_item["started_at"] = now_iso8601();

        // 완료 예정 시각 = 현재 시각 + total_production_time_hours
        {
            double total_h = j_item.value("total_production_time_hours", 0.0);
            auto est_t = std::time(nullptr)
                         + static_cast<std::time_t>(total_h * 3600.0);
            std::tm est_tm{};
            localtime_s(&est_tm, &est_t);
            std::ostringstream oss;
            oss << std::put_time(&est_tm, "%Y-%m-%dT%H:%M:%S");
            j_item["estimated_completion"] = oss.str();
        }

        save(arr);
        ProductionQueueItem p;
        from_json(j_item, p);
        return p;
    }
    throw std::out_of_range("Unknown production_id: " + production_id);
}

ProductionQueueItem ProductionQueueManager::start_with_quantities(
    const std::string& production_id,
    int    planned_qty,
    double total_production_time_hours)
{
    json arr = load();
    for (auto& j_item : arr) {
        if (j_item.at("production_id").get<std::string>() != production_id) continue;
        if (productionQueueStatusFromJson(j_item.at("status").get<std::string>())
                != ProductionQueueStatus::Waiting)
            throw std::runtime_error(
                "Cannot start: not Waiting. production_id=" + production_id);

        // 재계산된 생산량으로 업데이트
        j_item["planned_quantity"]            = planned_qty;
        j_item["total_production_time_hours"] = total_production_time_hours;
        j_item["status"]                      = toJsonString(ProductionQueueStatus::InProduction);
        j_item["started_at"]                  = now_iso8601();

        // 완료 예정 시각
        {
            auto est_t = std::time(nullptr)
                         + static_cast<std::time_t>(total_production_time_hours * 3600.0);
            std::tm est_tm{};
            localtime_s(&est_tm, &est_t);
            std::ostringstream oss;
            oss << std::put_time(&est_tm, "%Y-%m-%dT%H:%M:%S");
            j_item["estimated_completion"] = oss.str();
        }

        save(arr);
        ProductionQueueItem p;
        from_json(j_item, p);
        return p;
    }
    throw std::out_of_range("Unknown production_id: " + production_id);
}

ProductionQueueItem ProductionQueueManager::complete(const std::string& production_id) {
    json arr = load();
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        if ((*it).at("production_id").get<std::string>() != production_id) continue;
        if (productionQueueStatusFromJson((*it).at("status").get<std::string>())
                != ProductionQueueStatus::InProduction)
            throw std::runtime_error(
                "Cannot complete: not InProduction. production_id=" + production_id);
        ProductionQueueItem p;
        from_json(*it, p);
        arr.erase(it);
        save(arr);
        return p;
    }
    throw std::out_of_range("Unknown production_id: " + production_id);
}

void ProductionQueueManager::cancel(const std::string& production_id) {
    json arr = load();
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        if ((*it).at("production_id").get<std::string>() != production_id) continue;
        if (productionQueueStatusFromJson((*it).at("status").get<std::string>())
                != ProductionQueueStatus::Waiting)
            throw std::runtime_error(
                "Cannot cancel: only Waiting items can be cancelled. production_id=" + production_id);
        arr.erase(it);
        save(arr);
        return;
    }
    throw std::out_of_range("Unknown production_id: " + production_id);
}
