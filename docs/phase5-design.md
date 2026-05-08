# Phase 5 설계 문서 — View 계층

## 목표

**콘솔 입출력 UI** 구현. 비즈니스 로직 없이 표시·입력만 담당한다.  
Controller(Phase 6)가 View 메서드를 호출해 사용자와 인터랙션한다.

---

## 1. 의존성

```
View (Phase 5)
  └─→ Models/*.h   (SampleItem, InventoryItem, Order, ProductionQueueItem, Enums)
      표준 라이브러리 (<iostream>, <string>, <vector>)만 사용
      Service / Repository 의존 없음
```

---

## 2. 생성 파일 목록

```
SampleOrderSystem/
└── Views/
    ├── MainMenuView.h / .cpp   ← 공통 유틸 + 메인 메뉴
    ├── SampleView.h / .cpp     ← 메뉴 1: 시료 관리
    ├── OrderView.h / .cpp      ← 메뉴 2·3: 시료 주문·승인/거절
    ├── MonitoringView.h / .cpp ← 메뉴 4: 모니터링
    └── ProductionView.h / .cpp ← 메뉴 5·6: 출고 처리·생산 라인
```

수정 파일:
```
SampleOrderSystem/
├── SampleOrderSystem.vcxproj
├── SampleOrderSystem.vcxproj.filters
└── main.cpp                    ← ANSI VT 모드 활성화 추가
```

---

## 3. 공통 규칙

| 항목 | 규칙 |
|---|---|
| 콘솔 폭 | 80자 고정 |
| 인코딩 | UTF-8 (Phase 0에서 SetConsoleCP 설정 완료) |
| 한글 폭 계산 | UTF-8 3바이트 = 화면 2칸 (테이블 정렬에 반영) |
| ANSI 색상 | Windows 10+ VT 모드 활성화 전제 |
| 메서드 종류 | 전부 `static` — 상태 없음 |
| 비즈니스 로직 | 절대 포함 금지 |
| 입력 재시도 | 범위/타입 오류 시 재입력 요청 |
| 빈 목록 | "목록이 비어 있습니다." 메시지 출력 |

---

## 4. ANSI 색상 체계

DataMonitor 참고 레포 패턴을 그대로 적용한다.  
`Views/MainMenuView.h` 내부 `namespace Clr`에 정의.

```cpp
namespace Clr {
    constexpr const char* Reset    = "\033[0m";
    constexpr const char* Bold     = "\033[1m";
    constexpr const char* Dim      = "\033[2m";
    constexpr const char* Red      = "\033[31m";
    constexpr const char* BrRed    = "\033[91m";
    constexpr const char* BrYellow = "\033[93m";
    constexpr const char* BrGreen  = "\033[92m";
    constexpr const char* BrCyan   = "\033[96m";
    constexpr const char* BoldCyan = "\033[1;36m";
}
```

**OrderStatus → 색상 매핑**

| 상태 | 색상 | 표시 문자열 |
|---|---|---|
| RESERVED | 기본(Reset) | 주문 접수 |
| REJECTED | BrRed | 주문 거절 |
| PRODUCING | BrYellow | 생산 중 |
| CONFIRMED | BrGreen | 출고 대기 |
| RELEASE | BrCyan | 출고 완료 |

**ANSI VT 모드 활성화** (`main.cpp`에 추가):
```cpp
#include <windows.h>
// SetConsoleCP / SetConsoleOutputCP 이후
HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
DWORD dwMode = 0;
GetConsoleMode(hOut, &dwMode);
SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
```

---

## 5. UTF-8 폭 유틸리티

DataMonitor의 `utf8_width` / `utf8_clip` 패턴 적용.  
한글(3바이트 UTF-8) = 화면 2칸이므로 테이블 정렬 시 반드시 폭 기반으로 패딩해야 한다.

```cpp
// 화면 표시 너비 계산 (한글=2, ASCII=1)
static int  utf8DisplayWidth(const std::string& s);

// w 칸에 맞게 자르기 (우측 공백 채움)
static std::string padRight(const std::string& s, int w);

// w 칸에 맞게 왼쪽 공백 채움
static std::string padLeft(const std::string& s, int w);

// max_w 초과 시 끝에 '~' 붙여 자름
static std::string truncate(const std::string& s, int max_w);
```

---

## 6. MainMenuView

### 역할
- 메인 메뉴(0~6번) 출력 및 선택 입력
- 공통 유틸리티 제공 (나머지 View가 `#include "MainMenuView.h"`)

### API

```cpp
class MainMenuView {
public:
    // ── 메인 메뉴 ────────────────────────────────────────────────
    static void showMainMenu();
    static int  promptMenuChoice();   // 0~6, 범위 외 재입력

    // ── 공통 피드백 ───────────────────────────────────────────────
    static void showSuccess(const std::string& msg);  // 초록 ✓ 접두사
    static void showError(const std::string& msg);    // 빨강 ✗ 접두사
    static void pause();                              // Enter 대기

    // ── 레이아웃 유틸 ─────────────────────────────────────────────
    static void printHeader(const std::string& title);
    static void printLine(char ch = '-', int width = 80);
    static void clearScreen();

    // ── 입력 유틸 ─────────────────────────────────────────────────
    static std::string prompt(const std::string& label);
    static int         promptInt(const std::string& label);  // 숫자 아닌 경우 재입력
    static double      promptDouble(const std::string& label);

    // ── 문자열 유틸 (UTF-8 폭 기반) ──────────────────────────────
    static int         utf8DisplayWidth(const std::string& s);
    static std::string padRight(const std::string& s, int w);
    static std::string padLeft(const std::string& s, int w);
    static std::string truncate(const std::string& s, int max_w);
    static std::string colorStatus(OrderStatus status);
};
```

### 화면 레이아웃

```
================================================================================
                      s-semi 시료 관리 시스템
================================================================================
  1. 시료 관리
  2. 시료 주문
  3. 주문 승인/거절
  4. 모니터링
  5. 출고 처리
  6. 생산 라인
  0. 종료
--------------------------------------------------------------------------------
메뉴를 선택하세요: _
```

---

## 7. SampleView

### 역할
메뉴 1 (시료 관리) 전용 UI.

### 입력 구조체

```cpp
struct SampleInput {
    std::string sample_id;
    std::string sample_name;
    double      avg_production_time_hours;
    double      yield_rate;      // 0.0~1.0
    int         initial_quantity;
};
```

### API

```cpp
class SampleView {
public:
    static int         promptSubMenu();            // 1:등록 2:목록 3:검색 0:뒤로
    static SampleInput promptSampleInput();        // 시료 등록 폼
    static void showSampleTable(const std::vector<SampleItem>&    samples,
                                const std::vector<InventoryItem>& inventory);
    static std::string promptSearchKeyword();
    static void showSearchResults(const std::vector<SampleItem>& results,
                                  const std::vector<InventoryItem>& inventory);
};
```

### 화면 레이아웃

**서브메뉴**
```
================================================================================
                           시료 관리
================================================================================
  1. 시료 등록
  2. 목록 조회
  3. 이름 검색
  0. 뒤로
--------------------------------------------------------------------------------
```

**시료 목록 테이블** (열 폭: ID=8, 명칭=20, 생산시간=10, 수율=7, 재고=8)
```
--------------------------------------------------------------------------------
  번호  시료ID    시료명                생산시간(h)  수율    재고
  ----  --------  --------------------  -----------  ------  --------
     1  S-001     실리콘 웨이퍼 200mm        24.5    92.0%   100 EA
     2  S-002     GaN 기판                   10.0    85.0%     5 EA
--------------------------------------------------------------------------------
  총 2종
```

**시료 등록 폼**
```
================================================================================
                           시료 등록
================================================================================
  시료ID            : S-003
  시료명            : SiC 기판 6인치
  평균 생산시간 (h) : 36.0
  수율 (0.0~1.0)    : 0.88
  초기 재고 수량    : 0
--------------------------------------------------------------------------------
```

---

## 8. OrderView

### 역할
메뉴 2 (시료 주문) + 메뉴 3 (주문 승인/거절) UI.

### 입력 구조체

```cpp
struct OrderInput {
    std::string sample_id;
    std::string customer_name;
    int         order_quantity;
};
```

### API

```cpp
class OrderView {
public:
    // 메뉴 2: 주문 접수
    static OrderInput promptOrderInput();
    static void showPlaceOrderResult(const Order& order);

    // 메뉴 3: 승인/거절
    static int         promptApprovalSubMenu();    // 1:승인 2:거절 0:뒤로
    static void        showReservedOrders(const std::vector<Order>& orders);
    static std::string promptOrderNumber(const std::string& action);  // "승인" or "거절"
    static std::string promptRejectNote();
    static void showApproveResult(const Order& order);  // CONFIRMED or PRODUCING
    static void showRejectResult(const Order& order);
};
```

### 화면 레이아웃

**주문 접수 폼 (메뉴 2)**
```
================================================================================
                           시료 주문 접수
================================================================================
  시료ID    : S-001
  고객명    : 삼성전자
  주문수량  : 10
--------------------------------------------------------------------------------
```

**RESERVED 주문 목록 (메뉴 3)**
```
================================================================================
                        주문 승인/거절
================================================================================
[검토 대기 주문 목록]
  번호  주문번호            시료명                고객명       수량  접수 일시
  ----  ------------------  --------------------  -----------  ----  -------------------
     1  ORD-20260508-0001   실리콘 웨이퍼 200mm    삼성전자      10   2026-05-08T09:30:00
     2  ORD-20260508-0002   GaN 기판               SK하이닉스     5   2026-05-08T09:35:00
--------------------------------------------------------------------------------
  1. 승인    2. 거절    0. 뒤로
```

**승인 결과**
```
  ✓ [ORD-20260508-0001] 승인 완료
    → 재고 충분 — 출고 대기(CONFIRMED) 상태로 전환되었습니다.
  OR
    → 재고 부족 — 생산 큐에 등록되어 생산 중(PRODUCING) 상태입니다.
```

---

## 9. MonitoringView

### 역할
메뉴 4 (모니터링) 전용 UI. 읽기 전용 — 입력 없음.

### API

```cpp
class MonitoringView {
public:
    static void showDashboard(const std::vector<Order>&        orders,
                              const std::vector<SampleItem>&   samples,
                              const std::vector<InventoryItem>& inventory);
private:
    static void showOrderSummary(const std::vector<Order>& orders);
    static void showInventoryTable(const std::vector<SampleItem>&   samples,
                                   const std::vector<InventoryItem>& inventory);
};
```

### 화면 레이아웃

```
================================================================================
                            모니터링
================================================================================
[주문 현황]
  주문 접수   (RESERVED) :   2건
  생산 중     (PRODUCING):   1건
  출고 대기   (CONFIRMED):   3건
  출고 완료   (RELEASE)  :   1건
  거절됨      (REJECTED) :   0건
  ────────────────────────────────────
  합계                   :   7건

[재고 현황]
  시료ID    시료명                재고
  --------  --------------------  --------
  S-001     실리콘 웨이퍼 200mm    90 EA
  S-002     GaN 기판                0 EA  ← 재고 없음
--------------------------------------------------------------------------------
```

> 재고 0인 경우 빨간색 표시, 1~9 황색, 10 이상 초록.

---

## 10. ProductionView

### 역할
메뉴 5 (출고 처리) + 메뉴 6 (생산 라인) UI.

### API

```cpp
class ProductionView {
public:
    // 메뉴 5: 출고 처리
    static void        showConfirmedOrders(const std::vector<Order>& orders);
    static std::string promptShipOrderNumber();
    static void showShipResult(const Order& order);

    // 메뉴 6: 생산 라인
    static int  promptProductionSubMenu();   // 1:생산시작 2:생산완료 0:뒤로
    static void showProductionQueue(const std::vector<ProductionQueueItem>& queue);
    static void showStartResult(const ProductionQueueItem& item);
    static std::string promptCompleteProductionId();
    static void showCompleteResult(const Order& confirmedOrder);
};
```

### 화면 레이아웃

**출고 처리 (메뉴 5)**
```
================================================================================
                           출고 처리
================================================================================
[출고 대기 주문 목록 (CONFIRMED)]
  번호  주문번호            시료명                고객명       수량
  ----  ------------------  --------------------  -----------  ----
     1  ORD-20260508-0001   실리콘 웨이퍼 200mm    삼성전자      10
     2  ORD-20260508-0003   GaN 기판               SK하이닉스     5
--------------------------------------------------------------------------------
출고할 주문번호를 입력하세요 (0: 취소): _
```

**생산 라인 (메뉴 6)**
```
================================================================================
                           생산 라인
================================================================================
[생산 중 (InProduction)]
  생산ID              주문번호            시료명          수량  시작 시각
  ------------------  ------------------  --------------  ----  -------------------
  PROD-20260508-0001  ORD-20260508-0003   GaN 기판          10  2026-05-08T10:00:00

[대기 중 (Waiting) — FIFO 순서]
  순번  생산ID              주문번호            시료명          수량  등록 시각
  ----  ------------------  ------------------  --------------  ----  -------------------
     1  PROD-20260508-0002  ORD-20260508-0004   실리콘 웨이퍼    20   2026-05-08T10:05:00
--------------------------------------------------------------------------------
  1. 생산 시작 (Waiting → InProduction)
  2. 생산 완료 (InProduction → CONFIRMED)
  0. 뒤로
```

**생산 완료 결과**
```
  ✓ [PROD-20260508-0001] 생산 완료
    → 주문 [ORD-20260508-0003] 출고 대기(CONFIRMED) 상태로 전환되었습니다.
```

---

## 11. 프로젝트 파일 변경 사항

### 11.1 `SampleOrderSystem.vcxproj`

```xml
<ItemGroup>
  <ClCompile Include="Views\MainMenuView.cpp" />
  <ClCompile Include="Views\SampleView.cpp" />
  <ClCompile Include="Views\OrderView.cpp" />
  <ClCompile Include="Views\MonitoringView.cpp" />
  <ClCompile Include="Views\ProductionView.cpp" />
</ItemGroup>
<ItemGroup>
  <ClInclude Include="Views\MainMenuView.h" />
  <ClInclude Include="Views\SampleView.h" />
  <ClInclude Include="Views\OrderView.h" />
  <ClInclude Include="Views\MonitoringView.h" />
  <ClInclude Include="Views\ProductionView.h" />
</ItemGroup>
```

### 11.2 `SampleOrderSystem.vcxproj.filters`

```xml
<Filter Include="Views">
  <UniqueIdentifier>{A1B2C3D4-0005-0005-0005-000000000005}</UniqueIdentifier>
</Filter>
```

### 11.3 `main.cpp` 변경

```cpp
#include <iostream>
#include <windows.h>

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // ANSI VT 색상 코드 활성화 (Windows 10+)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    std::cout << "s-semi 시료 관리 시스템 시작" << std::endl;

    // AppController는 Phase 6에서 연결
    // AppController app;
    // app.run();

    return 0;
}
```

---

## 12. 핵심 설계 결정

| 항목 | 결정 | 이유 |
|---|---|---|
| 공통 유틸 위치 | `MainMenuView` static 메서드 | 나머지 View가 `#include "MainMenuView.h"` 하나로 유틸 사용 |
| UTF-8 폭 계산 | DataMonitor `utf8_width` 패턴 적용 | 한글 2칸 처리 없으면 테이블 정렬이 깨짐 |
| 입력 구조체 | View 헤더에 정의 (`SampleInput`, `OrderInput`) | 입력 데이터는 View와 Controller 경계에만 존재 |
| ANSI 색상 | `namespace Clr` in MainMenuView.h | 모든 View가 동일한 색상 체계 공유 |
| View 메서드 | 전부 `static` | 상태 없음, 인스턴스 불필요 |
| 자동 테스트 | 없음 | UI 출력은 콘솔 직접 확인 (Phase 7 통합 검증) |
| 빈 목록 처리 | 각 View에서 "목록이 비어 있습니다." 메시지 | 빈 테이블보다 명확한 피드백 |
| 서브메뉴 반환 | `int` (0=뒤로) | Controller가 switch로 분기 처리 |

---

## 13. 완료 기준 (Definition of Done)

| 번호 | 검증 항목 |
|---|---|
| 1 | 빌드 오류 없음 |
| 2 | 메인 메뉴 0~6번 올바르게 출력, 범위 외 입력 재요청 |
| 3 | 시료 등록 폼 입력 후 SampleInput 구조체 정확히 반환 |
| 4 | 시료 목록 테이블에서 한글 포함 시 정렬 유지 |
| 5 | OrderStatus 색상 구분 화면 출력 확인 |
| 6 | 승인 결과(CONFIRMED/PRODUCING 분기) 메시지 출력 확인 |
| 7 | 모니터링 재고 현황 색상 (0=빨강, 1~9=황색, 10+=초록) |
| 8 | 생산 큐 InProduction / Waiting 섹션 분리 출력 |
| 9 | 빈 목록 상황에서 "비어 있습니다." 메시지 정상 출력 |

---

## 14. 작업 순서 체크리스트

```
[ ] 1. SampleOrderSystem/Views/ 디렉토리 생성
[ ] 2. Views/MainMenuView.h 작성
      [ ] 2-1. namespace Clr 색상 상수
      [ ] 2-2. static 유틸 메서드 선언 (utf8DisplayWidth, padRight, padLeft, truncate)
      [ ] 2-3. showMainMenu, promptMenuChoice, showSuccess, showError, pause
[ ] 3. Views/MainMenuView.cpp 작성 (유틸 + 메인 메뉴 구현)
[ ] 4. Views/SampleView.h / .cpp 작성 (SampleInput, promptSubMenu, 테이블)
[ ] 5. Views/OrderView.h / .cpp 작성 (OrderInput, promptApprovalSubMenu, 목록)
[ ] 6. Views/MonitoringView.h / .cpp 작성 (showDashboard)
[ ] 7. Views/ProductionView.h / .cpp 작성 (출고 목록, 생산 큐)
[ ] 8. SampleOrderSystem.vcxproj — ClCompile/ClInclude 10개 추가
[ ] 9. SampleOrderSystem.vcxproj.filters — Views 필터 추가
[ ] 10. main.cpp — ANSI VT 모드 활성화 추가
[ ] 11. 빌드 확인 (자동 테스트 없음, Phase 7에서 통합 검증)
```
