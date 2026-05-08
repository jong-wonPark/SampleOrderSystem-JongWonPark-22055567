#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../SampleOrderSystem/Persistence/DataPersistence.h"

namespace fs = std::filesystem;

// ── 전체 데이터 스냅샷 ────────────────────────────────────────────

struct MonitorSnapshot {
    std::vector<SampleItem>          samples;
    std::vector<InventoryItem>       inventory;
    std::vector<Order>               orders;
    std::vector<ProductionQueueItem> queue;
    std::string                      captured_at;
    bool                             has_error = false;
    std::string                      error_msg;
};

class DataMonitor {
public:
    explicit DataMonitor(fs::path data_dir, int refresh_sec = 3);
    ~DataMonitor();

    void run();  // 블로킹 메인 루프

private:
    fs::path          data_dir_;
    int               refresh_sec_;

    MonitorSnapshot   snapshot_;
    std::mutex        snapshot_mutex_;
    std::atomic<bool> running_{ true };
    std::atomic<bool> data_ready_{ false };
    std::atomic<bool> force_refresh_{ false };
    std::condition_variable refresh_cv_;
    std::mutex              refresh_cv_mutex_;
    std::thread       bg_thread_;

    enum class View { Dashboard, Inventory, Orders, Production };
    View        current_view_ = View::Dashboard;
    std::string order_filter_;  // "" = 전체 / "RESERVED" 등 toJsonString 값

    void bg_refresh_loop();
    MonitorSnapshot load_snapshot() const;

    void display(const MonitorSnapshot& snap) const;
    void show_header(const MonitorSnapshot& snap) const;
    void show_nav() const;
    void show_dashboard(const MonitorSnapshot& snap) const;
    void show_inventory(const MonitorSnapshot& snap) const;
    void show_orders(const MonitorSnapshot& snap) const;
    void show_production(const MonitorSnapshot& snap) const;
    void show_footer() const;

    void handle_input(char c);

    // ── 정적 유틸 ────────────────────────────────────────────────
    static void        clear_screen();
    static void        hline(char ch = '-', int w = 80);
    static int         utf8_width(const std::string& s);
    static std::string rpad(const std::string& s, int w);
    static std::string lpad(const std::string& s, int w);
    static std::string trunc(const std::string& s, int max_w);
    static std::string colored_status(OrderStatus s);
    static std::string now_str();
};
