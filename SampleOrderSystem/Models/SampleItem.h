#pragma once
#include <string>

struct SampleItem {
    std::string sample_id;                       // 시료ID (예: "S-001")
    std::string sample_name;                     // 시료명 (예: "실리콘 웨이퍼 200mm")
    double      avg_production_time_hours = 0.0; // 평균 생산 시간(시)
    double      yield_rate               = 0.0;  // 수율 (0.0 ~ 1.0)
};
