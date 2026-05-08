# Phase 1 설계 문서 — Model 계층

## 목표

시스템 전체가 공유할 **데이터 구조**를 정의한다.  
이 단계 이후 모든 계층(Persistence, Repository, Service, Controller, View)이  
여기서 정의한 타입을 참조한다.

- 5개 헤더 파일 작성 (`Enums.h`, `SampleItem.h`, `InventoryItem.h`, `Order.h`, `ProductionQueueItem.h`)
- Model은 **순수 데이터 구조** + **순수 계산 메서드**만 포함. 비즈니스 로직·JSON 직렬화 없음
- vcxproj / vcxproj.filters에 `Models` 필터 등록

---

## 1. 의존성

```
Models (Phase 1)
  └─ 외부 의존 없음 (표준 라이브러리 <string>만 사용)

Persistence (Phase 2) ──→ Models
Repository  (Phase 3) ──→ Models
Service     (Phase 4) ──→ Models
Controller  (Phase 6) ──→ Models
View        (Phase 5) ──→ Models (toString() 사용)
```

---

## 2. 생성 파일 목록

```
SampleOrderSystem/
└── Models/
    ├── Enums.h              ← OrderStatus, ProductionQueueStatus + 변환 함수
    ├── SampleItem.h         ← 시료 마스터
    ├── InventoryItem.h      ← 재고 현황
    ├── Order.h              ← 고객 주문
    └── ProductionQueueItem.h ← 생산 큐 항목
```

수정 파일:
```
SampleOrderSystem/
├── SampleOrderSystem.vcxproj         ← <ClInclude> 5개 추가
└── SampleOrderSystem.vcxproj.filters ← Models 필터 추가, packages.config 항목 제거
```

---

## 3. 파일별 상세 설계

### 3.1 `Models/Enums.h`

**역할**: 모든 열거형 정의 및 변환 함수.  
`toString()` → 한국어 UI 표시 문자열  
`toJsonString()` / `*FromJson()` → Phase 2 Persistence 계층이 JSON 직렬화에 사용

```cpp
#pragma once
#include <string>

// ── 주문 상태 ──────────────────────────────────────────────────────

enum class OrderStatus {
    RESERVED,   // 주문 접수 (검토 대기 포함)
    REJECTED,   // 주문 거절
    PRODUCING,  // 승인 완료, 재고 부족으로 생산 중
    CONFIRMED,  // 승인 완료, 출고 대기 중
    RELEASE     // 출고 완료
};

// ── 생산 큐 상태 ───────────────────────────────────────────────────

enum class ProductionQueueStatus {
    Waiting,      // 생산 대기
    InProduction  // 생산 중
};

// ── 화면 표시용 (한국어) ──────────────────────────────────────────

inline std::string toString(OrderStatus s) {
    switch (s) {
        case OrderStatus::RESERVED:  return "주문 접수";
        case OrderStatus::REJECTED:  return "주문 거절";
        case OrderStatus::PRODUCING: return "생산 중";
        case OrderStatus::CONFIRMED: return "출고 대기";
        case OrderStatus::RELEASE:   return "출고 완료";
        default:                     return "알수없음";
    }
}

inline std::string toString(ProductionQueueStatus s) {
    switch (s) {
        case ProductionQueueStatus::Waiting:      return "대기";
        case ProductionQueueStatus::InProduction: return "생산 중";
        default:                                  return "알수없음";
    }
}

// ── JSON 직렬화용 (Phase 2 Persistence에서 사용) ──────────────────

inline std::string toJsonString(OrderStatus s) {
    switch (s) {
        case OrderStatus::RESERVED:  return "RESERVED";
        case OrderStatus::REJECTED:  return "REJECTED";
        case OrderStatus::PRODUCING: return "PRODUCING";
        case OrderStatus::CONFIRMED: return "CONFIRMED";
        case OrderStatus::RELEASE:   return "RELEASE";
        default:                     return "RESERVED";
    }
}

inline OrderStatus orderStatusFromJson(const std::string& s) {
    if (s == "REJECTED")  return OrderStatus::REJECTED;
    if (s == "PRODUCING") return OrderStatus::PRODUCING;
    if (s == "CONFIRMED") return OrderStatus::CONFIRMED;
    if (s == "RELEASE")   return OrderStatus::RELEASE;
    return OrderStatus::RESERVED;
}

inline std::string toJsonString(ProductionQueueStatus s) {
    return (s == ProductionQueueStatus::InProduction) ? "InProduction" : "Waiting";
}

inline ProductionQueueStatus productionQueueStatusFromJson(const std::string& s) {
    return (s == "InProduction") ? ProductionQueueStatus::InProduction
                                 : ProductionQueueStatus::Waiting;
}
```

---

### 3.2 `Models/SampleItem.h`

**역할**: 시료 마스터 정보. 시료 관리 메뉴(메뉴 1)의 핵심 엔티티.

```cpp
#pragma once
#include <string>

struct SampleItem {
    std::string sample_id;                       // 시료ID (예: "S-001")
    std::string sample_name;                     // 시료명 (예: "실리콘 웨이퍼 200mm")
    double      avg_production_time_hours = 0.0; // 평균 생산 시간(시)
    double      yield_rate               = 0.0;  // 수율 (0.0 ~ 1.0)
};
```

---

### 3.3 `Models/InventoryItem.h`

**역할**: 시료별 현재 재고 수량. `sample_id`로 `SampleItem`과 1:1 연결.

```cpp
#pragma once
#include <string>

struct InventoryItem {
    std::string sample_id;
    int         quantity     = 0;    // 현재 재고 수량
    std::string unit         = "EA"; // 단위
    std::string last_updated;        // 최종 갱신 일시 (ISO 8601)
};
```

> **설계 결정**: 재고 예약(`reservedQuantity`)은 이 시스템에서 사용하지 않는다.  
> 주문 승인 시 재고를 즉시 차감하는 방식이므로 예약 개념이 불필요하다.

---

### 3.4 `Models/Order.h`

**역할**: 고객 주문. 시스템의 핵심 엔티티로, 전체 상태 전이(RESERVED→RELEASE)의 주체.

```cpp
#pragma once
#include <string>
#include "Enums.h"

struct Order {
    std::string order_number;                          // ORD-YYYYMMDD-XXXX (자동 채번)
    std::string sample_id;
    std::string sample_name;                           // 비정규화 — 표시용 캐시
    std::string customer_name;
    int         order_quantity = 0;
    OrderStatus status         = OrderStatus::RESERVED;
    std::string order_date;                            // 접수 일시 (ISO 8601)
    std::string note;                                  // 거절 사유 등 메모
};
```

> **설계 결정**: `sample_name`을 Order에 중복 저장(비정규화)한다.  
> 시료 마스터가 변경되어도 과거 주문의 표시명이 유지되어야 하기 때문이다.

---

### 3.5 `Models/ProductionQueueItem.h`

**역할**: 생산 라인 큐의 항목. `PRODUCING` 상태 주문과 1:1 대응.  
`queued_at` 기준 오름차순 정렬이 FIFO 보장의 핵심이다.

```cpp
#pragma once
#include <string>
#include "Enums.h"

struct ProductionQueueItem {
    std::string           production_id;                            // PROD-YYYYMMDD-XXXX
    std::string           order_number;                             // 연결된 주문번호
    std::string           sample_id;
    std::string           sample_name;
    int                   planned_quantity = 0;
    ProductionQueueStatus status           = ProductionQueueStatus::Waiting;
    std::string           queued_at;        // 큐 등록 일시 (ISO 8601) — FIFO 정렬 기준
    std::string           started_at;       // 생산 시작 일시 (빈 문자열 = 미시작)
};
```

---

## 4. 프로젝트 파일 변경 사항

### 4.1 `SampleOrderSystem.vcxproj` — `<ClInclude>` 추가

기존 `<ItemGroup>` (main.cpp를 포함하는 `<ClCompile>`) 아래에 헤더 그룹 추가:

```xml
<ItemGroup>
  <ClInclude Include="Models\Enums.h" />
  <ClInclude Include="Models\SampleItem.h" />
  <ClInclude Include="Models\InventoryItem.h" />
  <ClInclude Include="Models\Order.h" />
  <ClInclude Include="Models\ProductionQueueItem.h" />
</ItemGroup>
```

### 4.2 `SampleOrderSystem.vcxproj.filters` — Models 필터 추가

```xml
<!-- 필터 목록에 추가 -->
<Filter Include="Models">
  <UniqueIdentifier>{A1B2C3D4-0001-0001-0001-000000000001}</UniqueIdentifier>
</Filter>

<!-- 헤더 파일 항목에 추가 -->
<ClInclude Include="Models\Enums.h">
  <Filter>Models</Filter>
</ClInclude>
<ClInclude Include="Models\SampleItem.h">
  <Filter>Models</Filter>
</ClInclude>
<ClInclude Include="Models\InventoryItem.h">
  <Filter>Models</Filter>
</ClInclude>
<ClInclude Include="Models\Order.h">
  <Filter>Models</Filter>
</ClInclude>
<ClInclude Include="Models\ProductionQueueItem.h">
  <Filter>Models</Filter>
</ClInclude>

<!-- 삭제: 이미 제거된 packages.config 참조 -->
<!-- <None Include="packages.config" /> 제거 -->
```

---

## 5. 핵심 설계 결정

| 항목 | 결정 | 이유 |
|---|---|---|
| 멤버 변수 명명 | `snake_case` | DataPersistence 참고 레포 패턴과 동일, JSON 키와 일치 |
| 숫자 멤버 초기화 | `= 0`, `= 0.0` | 미초기화 UB 방지 |
| 문자열 멤버 초기화 | 불필요 (기본 빈 문자열) | `std::string` 기본 생성자가 처리 |
| JSON 직렬화 위치 | Models에 없음, Phase 2로 위임 | Model이 nlohmann/json에 의존하지 않도록 분리 |
| toString/toJsonString 위치 | `Enums.h`에 inline | enum과 문자열 변환을 한 파일에서 관리, 헤더 온리 |
| `sample_name` 비정규화 | Order, ProductionQueueItem에 포함 | 시료 마스터 변경 시에도 과거 기록 표시명 유지 |
| 재고 예약 필드 없음 | InventoryItem에 reservedQuantity 없음 | 승인 시 즉시 차감 방식이므로 예약 단계 불필요 |

---

## 6. 완료 기준 (Definition of Done)

| 번호 | 검증 항목 | 확인 방법 |
|---|---|---|
| 1 | 빌드 오류 없음 | msbuild Debug x64 성공 |
| 2 | 모든 5개 헤더 파일 생성 | 파일 시스템 확인 |
| 3 | `#include "Models/Order.h"` in main.cpp 컴파일 성공 | 임시 include 추가 후 빌드 |
| 4 | `toString(OrderStatus::CONFIRMED)` → `"출고 대기"` 반환 | 임시 출력 코드로 확인 |
| 5 | `toJsonString` / `FromJson` 왕복 변환 정확성 | 5개 값 모두 확인 |
| 6 | VS Solution Explorer에서 Models 필터 표시 | IDE에서 확인 |

---

## 7. 작업 순서 체크리스트

```
[ ] 1. SampleOrderSystem/Models/ 디렉토리 생성
[ ] 2. Models/Enums.h 작성
[ ] 3. Models/SampleItem.h 작성
[ ] 4. Models/InventoryItem.h 작성
[ ] 5. Models/Order.h 작성
[ ] 6. Models/ProductionQueueItem.h 작성
[ ] 7. SampleOrderSystem.vcxproj — <ClInclude> 5개 추가
[ ] 8. SampleOrderSystem.vcxproj.filters — Models 필터 추가, packages.config 항목 제거
[ ] 9. 빌드 및 완료 기준 검증
```
