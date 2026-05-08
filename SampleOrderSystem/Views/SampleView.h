#pragma once
#include <string>
#include <vector>

#include "../Models/SampleItem.h"
#include "../Models/InventoryItem.h"
#include "MainMenuView.h"

struct SampleInput {
    std::string sample_id;
    std::string sample_name;
    double      avg_production_time_hours;
    double      yield_rate;        // 0.0 ~ 1.0
    int         initial_quantity;
};

class SampleView {
public:
    static int         promptSubMenu();    // 1:등록 2:목록 3:검색 0:뒤로
    static SampleInput promptSampleInput();
    static void showSampleTable(const std::vector<SampleItem>&    samples,
                                const std::vector<InventoryItem>& inventory);
    static std::string promptSearchKeyword();
    static void showSearchResults(const std::vector<SampleItem>&    results,
                                  const std::vector<InventoryItem>& inventory);
};
