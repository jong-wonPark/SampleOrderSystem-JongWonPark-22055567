# Phase 2 설계 문서 — Persistence 계층

## 목표

JSON 파일 ↔ Model 구조체 직렬화/역직렬화 레이어 구현.  
Repository(Phase 3)가 이 Manager 클래스들을 통해서만 파일 I/O를 수행한다.

- `Persistence/DataPersistence.h` — Manager 3종 선언 + to_json/from_json 선언
- `Persistence/DataPersistence.cpp` — 구현체
- vcxproj / vcxproj.filters에 Persistence 필터 등록

---

## 1. 의존성

```
Persistence (Phase 2)
  ├─→ Models/Enums.h          (OrderStatus, ProductionQueueStatus 변환 함수)
  ├─→ Models/SampleItem.h
  ├─→ Models/InventoryItem.h
  ├─→ Models/Order.h
  ├─→ Models/ProductionQueueItem.h
  ├─→ <nlohmann/json.hpp>
  └─→ 표준 라이브러리 (<filesystem>, <fstream>, <optional>, <vector>, ...)

Repository (Phase 3) ──→ Persistence
```

---

## 2. 생성 파일 목록

```
SampleOrderSystem/
└── Persistence/
    ├── DataPersistence.h    ← Manager 선언, to_json/from_json 선언
    └── DataPersistence.cpp  ← 구현체
```

수정 파일:
```
SampleOrderSystem/
├── SampleOrderSystem.vcxproj         ← ClCompile/ClInclude 추가
└── SampleOrderSystem.vcxproj.filters ← Persistence 필터 추가
```

---

## 3. JSON 데이터 스키마

### 3.1 `inventory.json` — 배열, 항목당 SampleItem + InventoryItem 병합

```json
[
  {
    "sample_id"                : "S-001",
    "sample_name"              : "실리콘 웨이퍼 200mm",
    "avg_production_time_hours": 24.5,
    "yield_rate"               : 0.92,
    "quantity"                 : 100,
    "unit"                     : "EA",
    "last_updated"             : "2026-05-08T10:00:00"
  }
]
```

> SampleItem과 InventoryItem은 sample_id 기준 1:1이므로 단일 JSON 레코드로 병합 저장한다.  
> 두 구조체의 분리된 뷰는 Manager가 제공한다.

### 3.2 `orders.json`

```json
[
  {
    "order_number"  : "ORD-20260508-0001",
    "sample_id"     : "S-001",
    "sample_name"   : "실리콘 웨이퍼 200mm",
    "customer_name" : "삼성전자",
    "order_quantity": 10,
    "status"        : "RESERVED",
    "order_date"    : "2026-05-08T09:30:00",
    "note"          : ""
  }
]
```

`status` 허용값: `"RESERVED"` `"REJECTED"` `"PRODUCING"` `"CONFIRMED"` `"RELEASE"`

### 3.3 `production_queue.json`

```json
[
  {
    "production_id"  : "PROD-20260508-0001",
    "order_number"   : "ORD-20260508-0001",
    "sample_id"      : "S-001",
    "sample_name"    : "실리콘 웨이퍼 200mm",
    "planned_quantity": 10,
    "status"         : "Waiting",
    "queued_at"      : "2026-05-08T09:31:00",
    "started_at"     : ""
  }
]
```

`status` 허용값: `"Waiting"` `"InProduction"`  
`queued_at` 기준 오름차순 정렬이 FIFO의 핵심이다.

---

## 4. 내부 유틸리티 (DataPersistence.cpp 파일 스코프 static)

```cpp
// ISO 8601 현재 시각 — 타임스탬프 필드에 사용
static std::string now_iso8601();

// YYYYMMDD 오늘 날짜 — ID 채번 접두사에 사용
static std::string today_str();

// JSON 파일 읽기 (없으면 빈 배열 반환)
static json load_array(const fs::path& path);

// JSON 파일 쓰기 (들여쓰기 2칸)
static void save_array(const fs::path& path, const json& arr);
```

**구현 요점**:
- `localtime_s` (MSVC 전용) 사용 → Windows 단일 플랫폼
- 파일 없음 → `json::array()` 반환 (예외 아님)
- 파싱 오류 → `std::runtime_error` 발생
- `save_array`: `fs::create_directories`로 경로 자동 생성

---

## 5. 직렬화 설계 (to_json / from_json)

nlohmann/json의 ADL(Argument-Dependent Lookup) 방식.  
Model 헤더가 nlohmann/json에 의존하지 않도록 **DataPersistence.h에 선언, .cpp에 구현**.

### 5.1 Order

```cpp
// status 필드는 toJsonString() / orderStatusFromJson() 경유
void to_json(json& j, const Order& o) {
    j = {
        {"order_number",   o.order_number},
        {"sample_id",      o.sample_id},
        {"sample_name",    o.sample_name},
        {"customer_name",  o.customer_name},
        {"order_quantity", o.order_quantity},
        {"status",         toJsonString(o.status)},   // ← enum → string
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
    o.status = orderStatusFromJson(j.at("status").get<std::string>());  // ← string → enum
    j.at("order_date").get_to(o.order_date);
    j.at("note").get_to(o.note);
}
```

### 5.2 ProductionQueueItem

```cpp
// status 필드는 toJsonString() / productionQueueStatusFromJson() 경유
// started_at 빈 문자열 허용 (null 아님, 빈 string으로 저장)
void to_json(json& j, const ProductionQueueItem& p);
void from_json(const json& j, ProductionQueueItem& p);
```

### 5.3 inventory.json 레코드 (Manager 내부 처리)

SampleItem + InventoryItem 병합 레코드는 별도 struct 없이 Manager 메서드 내에서 직접 json 객체로 조립/분해:

```cpp
// 조립 (add_sample 내부)
json record = {
    {"sample_id",                 s.sample_id},
    {"sample_name",               s.sample_name},
    {"avg_production_time_hours", s.avg_production_time_hours},
    {"yield_rate",                s.yield_rate},
    {"quantity",                  initial_quantity},
    {"unit",                      unit},
    {"last_updated",              now_iso8601()}
};

// SampleItem 뷰 추출
SampleItem item;
item.sample_id                 = rec["sample_id"];
item.sample_name               = rec["sample_name"];
item.avg_production_time_hours = rec["avg_production_time_hours"];
item.yield_rate                = rec["yield_rate"];

// InventoryItem 뷰 추출
InventoryItem inv;
inv.sample_id    = rec["sample_id"];
inv.quantity     = rec["quantity"];
inv.unit         = rec["unit"];
inv.last_updated = rec["last_updated"];
```

---

## 6. Manager 클래스 API

### 6.1 InventoryManager

```cpp
class InventoryManager {
public:
    explicit InventoryManager(fs::path file_path);

    // ── Create ────────────────────────────────────────────────────
    // 시료 등록 + 초기 재고. 중복 sample_id 시 예외
    SampleItem add_sample(const std::string& sample_id,
                          const std::string& sample_name,
                          double avg_production_time_hours,
                          double yield_rate,
                          int    initial_quantity = 0,
                          const std::string& unit = "EA");

    // ── Read (SampleItem 관점) ────────────────────────────────────
    std::vector<SampleItem>   get_all_samples() const;
    std::optional<SampleItem> get_sample_by_id(const std::string& sample_id) const;
    // sample_name 부분 일치(대소문자 무시) 검색
    std::vector<SampleItem>   find_samples_by_name(const std::string& keyword) const;

    // ── Read (InventoryItem 관점) ─────────────────────────────────
    std::vector<InventoryItem>   get_all_inventory() const;
    std::optional<InventoryItem> get_inventory_by_id(const std::string& sample_id) const;

    // ── Update ────────────────────────────────────────────────────
    // delta > 0: 입고 / delta < 0: 출고 (잔량 부족 시 예외)
    InventoryItem update_stock(const std::string& sample_id, int delta);

    // ── Delete ────────────────────────────────────────────────────
    void remove_sample(const std::string& sample_id);

private:
    fs::path file_path_;
    json load() const;
    void save(const json& arr) const;
};
```

### 6.2 OrderManager

```cpp
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
    // note: 거절 사유 등 (선택, 기본 빈 문자열)
    Order update_status(const std::string& order_number,
                        OrderStatus        new_status,
                        const std::string& note = "");

    // ── Delete ────────────────────────────────────────────────────
    void remove(const std::string& order_number);

private:
    fs::path file_path_;
    json load() const;
    void save(const json& arr) const;
    std::string new_order_number(const json& arr) const; // ORD-YYYYMMDD-XXXX
};
```

### 6.3 ProductionQueueManager

```cpp
class ProductionQueueManager {
public:
    explicit ProductionQueueManager(fs::path file_path);

    // ── Create ────────────────────────────────────────────────────
    // production_id 자동 채번 (PROD-YYYYMMDD-XXXX), status = Waiting
    ProductionQueueItem enqueue(const std::string& order_number,
                                const std::string& sample_id,
                                const std::string& sample_name,
                                int                planned_quantity);

    // ── Read ──────────────────────────────────────────────────────
    // 모두 queued_at ASC 정렬 (FIFO 보장)
    std::vector<ProductionQueueItem>   get_all() const;
    std::optional<ProductionQueueItem> get_by_id(const std::string& production_id) const;
    std::vector<ProductionQueueItem>   get_by_status(ProductionQueueStatus status) const;
    // Waiting 항목 중 queued_at 가장 이른 것 반환
    std::optional<ProductionQueueItem> get_front() const;

    // ── Update ────────────────────────────────────────────────────
    // Waiting → InProduction (Waiting 아니면 예외)
    ProductionQueueItem start(const std::string& production_id);
    // 완료: 큐에서 제거 후 항목 반환 (InProduction 아니면 예외)
    ProductionQueueItem complete(const std::string& production_id);

    // ── Delete ────────────────────────────────────────────────────
    // Waiting 상태만 취소 가능
    void cancel(const std::string& production_id);

private:
    fs::path file_path_;
    json load() const;
    void save(const json& arr) const;
    std::string new_production_id(const json& arr) const; // PROD-YYYYMMDD-XXXX
};
```

---

## 7. 에러 처리 전략

| 상황 | 예외 타입 | 예시 |
|---|---|---|
| 파일 열기 실패 | `std::runtime_error` | `"Cannot open file: data/orders.json"` |
| JSON 파싱 오류 | `std::runtime_error` (nlohmann 예외 래핑) | 파일 내용 손상 시 |
| 중복 sample_id | `std::invalid_argument` | `add_sample` 중복 호출 |
| 존재하지 않는 ID | `std::out_of_range` | `update_status` / `start` / `cancel` |
| 재고 부족 (출고) | `std::runtime_error` | `update_stock(-50)` 잔량 < 50 |
| 잘못된 상태 전이 | `std::runtime_error` | Waiting 아닌 항목에 `start()` 호출 |

Repository 계층이 이 예외를 catch하여 `bool` 또는 `std::optional`로 변환한다.

---

## 8. 프로젝트 파일 변경 사항

### 8.1 `SampleOrderSystem.vcxproj`

기존 ClInclude/ClCompile ItemGroup에 추가:

```xml
<ItemGroup>
  <ClCompile Include="Persistence\DataPersistence.cpp" />
</ItemGroup>
<ItemGroup>
  <ClInclude Include="Persistence\DataPersistence.h" />
</ItemGroup>
```

### 8.2 `SampleOrderSystem.vcxproj.filters`

```xml
<Filter Include="Persistence">
  <UniqueIdentifier>{A1B2C3D4-0002-0002-0002-000000000002}</UniqueIdentifier>
</Filter>

<ClCompile Include="Persistence\DataPersistence.cpp">
  <Filter>Persistence</Filter>
</ClCompile>

<ClInclude Include="Persistence\DataPersistence.h">
  <Filter>Persistence</Filter>
</ClInclude>
```

---

## 9. 핵심 설계 결정

| 항목 | 결정 | 이유 |
|---|---|---|
| JSON 루트 형식 | 배열 `[]` (bare array) | Phase 0에서 생성한 초기 파일 형식 유지 |
| inventory.json 구조 | SampleItem + InventoryItem 병합 단일 레코드 | sample_id 기준 1:1 관계, 별도 파일보다 일관성 유지 용이 |
| to_json/from_json 위치 | DataPersistence.h 선언 / .cpp 구현 | Model 헤더가 nlohmann/json 의존 금지 |
| status 직렬화 | Enums.h의 `toJsonString()` / `*FromJson()` 사용 | enum 타입 안전성 유지, 문자열 하드코딩 방지 |
| started_at 빈값 | null 대신 빈 문자열 `""` | JSON null 처리 복잡도 제거, from_json 일관성 |
| FIFO 정렬 | `get_all()` 반환 시 `queued_at` ASC 정렬 | Repository가 별도 정렬 불필요 |
| 파일 경로 주입 | 생성자 `fs::path file_path` 파라미터 | 테스트 시 경로 교체 가능, 하드코딩 없음 |
| 검색 키워드 | `find_samples_by_name`: 대소문자 무시 부분 일치 | "웨이퍼"로 "실리콘 웨이퍼 200mm" 검색 지원 |

---

## 10. 완료 기준 (Definition of Done)

| 번호 | 검증 항목 | 확인 방법 |
|---|---|---|
| 1 | 빌드 오류 없음 | msbuild Debug x64 성공 |
| 2 | InventoryManager: 시료 등록 → JSON 파일 기록 | inventory.json 내용 확인 |
| 3 | InventoryManager: 재고 차감 → 잔량 부족 시 예외 | `update_stock(delta < -quantity)` |
| 4 | InventoryManager: 이름 검색 | `find_samples_by_name("웨이퍼")` 결과 확인 |
| 5 | OrderManager: 주문 추가 → ORD-YYYYMMDD-XXXX 채번 | order_number 형식 확인 |
| 6 | OrderManager: 상태 변경 → JSON 갱신 | `update_status` 후 파일 확인 |
| 7 | ProductionQueueManager: enqueue → FIFO 정렬 | `get_all()` 순서 확인 |
| 8 | ProductionQueueManager: start/complete 상태 전이 | Waiting→InProduction→제거 |
| 9 | 잘못된 상태 전이 시 예외 발생 | InProduction 항목에 `start()` 호출 |
| 10 | 앱 재시작 후 데이터 복구 | 파일 저장 후 Manager 재생성하여 조회 |

---

## 11. 작업 순서 체크리스트

```
[ ] 1. SampleOrderSystem/Persistence/ 디렉토리 생성
[ ] 2. Persistence/DataPersistence.h 작성
      [ ] 2-1. #include 목록 (Models/*.h, nlohmann/json.hpp, filesystem 등)
      [ ] 2-2. 네임스페이스 별칭 (using json = nlohmann::json; namespace fs = ...)
      [ ] 2-3. to_json / from_json 선언 (Order, ProductionQueueItem)
      [ ] 2-4. InventoryManager 클래스 선언
      [ ] 2-5. OrderManager 클래스 선언
      [ ] 2-6. ProductionQueueManager 클래스 선언
[ ] 3. Persistence/DataPersistence.cpp 작성
      [ ] 3-1. 내부 유틸리티 (now_iso8601, today_str, load_array, save_array)
      [ ] 3-2. to_json / from_json 구현 (Order, ProductionQueueItem)
      [ ] 3-3. InventoryManager 구현 (add_sample, get_all_*, find_by_name, update_stock, remove)
      [ ] 3-4. OrderManager 구현 (add, get_all/by_number/by_status, update_status, remove)
      [ ] 3-5. ProductionQueueManager 구현 (enqueue, get_all/by_id/by_status/front, start, complete, cancel)
[ ] 4. SampleOrderSystem.vcxproj — ClCompile/ClInclude 추가
[ ] 5. SampleOrderSystem.vcxproj.filters — Persistence 필터 추가
[ ] 6. 빌드 및 완료 기준 10가지 검증
```
