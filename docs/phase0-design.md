# Phase 0 설계 문서 — 프로젝트 기반 설정

## 목표

이후 모든 Phase(1~9)가 공유하는 빌드 환경을 구성한다.

- gmock 의존성 제거 후 콘솔 애플리케이션으로 전환
- C++20 + UTF-8 인코딩 컴파일러 옵션 적용
- nlohmann/json 헤더 라이브러리 배치 및 include 경로 등록
- 런타임 데이터 디렉토리(`data/`) 및 빈 JSON 파일 3개 생성
- 빌드 시 `data/`를 실행 파일 출력 디렉토리로 자동 복사하는 post-build 단계 추가
- 앱 진입점(`main.cpp`) 최소 골격 작성

---

## 1. 현재 상태 (Phase 0 시작 전)

| 항목 | 현재 상태 | 목표 상태 |
|---|---|---|
| 언어 표준 | C++20 (`stdcpp20`) ✅ | 유지 |
| UTF-8 컴파일 옵션 | 미설정 ❌ | `/utf-8` 추가 |
| nlohmann/json | 미포함 ❌ | `include/nlohmann/json.hpp` 배치 |
| AdditionalIncludeDirectories | 미설정 ❌ | `$(SolutionDir)include` 추가 |
| gmock NuGet 의존성 | 포함 ❌ | 제거 |
| `data/` 디렉토리 | 없음 ❌ | 3개 JSON 파일 생성 |
| post-build 복사 | 없음 ❌ | `data/` → 출력 디렉토리 복사 추가 |
| `main.cpp` | gmock 진입점 ❌ | 앱 골격으로 교체 |

---

## 2. 디렉토리 구조 (Phase 0 완료 후)

```
SampleOrderSystem/
├── SampleOrderSystem.slnx
├── docs/
│   ├── PRD.md
│   ├── phase0-design.md          ← 현재 문서
│   └── ...
├── include/
│   └── nlohmann/
│       └── json.hpp              ← v3.11.3, 헤더 온리
├── data/                         ← 런타임 JSON 저장소
│   ├── inventory.json
│   ├── orders.json
│   └── production_queue.json
└── SampleOrderSystem/
    ├── SampleOrderSystem.vcxproj ← 수정 대상
    ├── SampleOrderSystem.vcxproj.filters
    ├── main.cpp                  ← 교체 대상
    └── packages.config           ← 삭제 대상
```

---

## 3. vcxproj 변경 사항

### 3.1 gmock 의존성 제거

**삭제할 항목**:

```xml
<!-- packages.config 참조 제거 -->
<ItemGroup>
  <None Include="packages.config" />
</ItemGroup>

<!-- gmock.targets import 제거 -->
<Import Project="..\packages\gmock.1.11.0\build\native\gmock.targets"
        Condition="Exists(...)" />

<!-- EnsureNuGetPackageBuildImports 타겟 전체 제거 -->
<Target Name="EnsureNuGetPackageBuildImports" ...>
  ...
</Target>
```

### 3.2 컴파일러 옵션 추가

4개 구성(Debug/Release × Win32/x64) 각각의 `<ClCompile>` 블록에 아래 두 항목을 추가한다:

```xml
<!-- UTF-8 소스 파일 인코딩 -->
<AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>

<!-- nlohmann/json 헤더 경로 -->
<AdditionalIncludeDirectories>
  $(SolutionDir)include;%(AdditionalIncludeDirectories)
</AdditionalIncludeDirectories>
```

**예시 — Debug|x64 블록 목표 형태**:

```xml
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
  <ClCompile>
    <WarningLevel>Level3</WarningLevel>
    <SDLCheck>true</SDLCheck>
    <PreprocessorDefinitions>_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    <ConformanceMode>true</ConformanceMode>
    <LanguageStandard>stdcpp20</LanguageStandard>
    <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
    <AdditionalIncludeDirectories>$(SolutionDir)include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
  </ClCompile>
  <Link>
    <SubSystem>Console</SubSystem>
    <GenerateDebugInformation>true</GenerateDebugInformation>
  </Link>
</ItemDefinitionGroup>
```

### 3.3 post-build 단계 추가

빌드 완료 후 `data/` 디렉토리를 실행 파일 옆에 복사한다. 어느 구성에서든 실행 가능하도록 모든 구성 조건 외부에 단일 타겟으로 작성한다:

```xml
<Target Name="CopyDataDir" AfterTargets="Build">
  <ItemGroup>
    <DataFiles Include="$(SolutionDir)data\**\*.*" />
  </ItemGroup>
  <Copy
    SourceFiles="@(DataFiles)"
    DestinationFiles="@(DataFiles->'$(OutDir)data\%(RecursiveDir)%(Filename)%(Extension)')"
    SkipUnchangedFiles="true"
  />
</Target>
```

---

## 4. nlohmann/json 라이브러리 배치

- **출처**: [https://github.com/nlohmann/json](https://github.com/nlohmann/json) releases v3.11.3
- **파일**: `json.hpp` 단일 헤더 (약 1MB)
- **배치 경로**: `SampleOrderSystem/include/nlohmann/json.hpp`
- **사용 방법**:
  ```cpp
  #include <nlohmann/json.hpp>
  using json = nlohmann::json;
  ```
- `.gitignore`에는 포함하지 않음 — 외부 CMake/vcpkg 없이 단순 배포하기 위해 직접 커밋한다.

---

## 5. 초기 JSON 파일 스키마

세 파일 모두 **빈 JSON 배열**로 시작한다.  
Phase 2(Persistence)에서 각 Manager가 파일이 없거나 비어 있을 때 `[]`를 기본값으로 처리한다.  
Phase 8(DummyDataGenerator)에서 실제 테스트 데이터를 채운다.

### 5.1 `data/inventory.json`
```json
[]
```
향후 저장되는 객체 형태 (참고용):
```json
{
  "sample_id": "S-001",
  "sample_name": "실리콘 웨이퍼 200mm",
  "avg_production_time_hours": 24.5,
  "yield_rate": 0.92,
  "quantity": 100,
  "unit": "EA",
  "last_updated": "2026-05-08T10:00:00"
}
```

### 5.2 `data/orders.json`
```json
[]
```
향후 저장되는 객체 형태 (참고용):
```json
{
  "order_number": "ORD-20260508-0001",
  "sample_id": "S-001",
  "sample_name": "실리콘 웨이퍼 200mm",
  "customer_name": "삼성전자",
  "order_quantity": 10,
  "status": "RESERVED",
  "order_date": "2026-05-08T09:30:00",
  "note": ""
}
```

`status` 허용값: `"RESERVED"` `"REJECTED"` `"PRODUCING"` `"CONFIRMED"` `"RELEASE"`

### 5.3 `data/production_queue.json`
```json
[]
```
향후 저장되는 객체 형태 (참고용):
```json
{
  "production_id": "PROD-20260508-0001",
  "order_number": "ORD-20260508-0001",
  "sample_id": "S-001",
  "sample_name": "실리콘 웨이퍼 200mm",
  "planned_quantity": 10,
  "status": "Waiting",
  "queued_at": "2026-05-08T09:31:00",
  "started_at": ""
}
```

`status` 허용값: `"Waiting"` `"InProduction"`

---

## 6. main.cpp 목표 코드

```cpp
#include <iostream>
#include <windows.h>

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "s-semi 시료 관리 시스템 시작" << std::endl;

    // AppController는 Phase 6에서 연결
    // AppController app;
    // app.run();

    return 0;
}
```

- `SetConsoleCP` / `SetConsoleOutputCP`: 한글 콘솔 출력 보장
- `AppController::run()` 호출은 Phase 6 완성 후 주석 해제

---

## 7. 완료 기준 (Definition of Done)

| 번호 | 검증 항목 | 확인 방법 |
|---|---|---|
| 1 | Visual Studio에서 빌드 오류 없이 성공 | Build → Build Solution (F7) |
| 2 | 실행 파일 옆에 `data/` 폴더 3개 JSON 파일 자동 복사 | 출력 디렉토리 확인 |
| 3 | 실행 시 콘솔에 한글 출력 정상 표시 | `"s-semi 시료 관리 시스템 시작"` 출력 확인 |
| 4 | `#include <nlohmann/json.hpp>` 컴파일 성공 | main.cpp에 임시 include 추가 후 빌드 |
| 5 | gmock 관련 오류 없음 | 빌드 출력 로그 확인 |

---

## 8. 작업 순서 체크리스트

```
[ ] 1. include/nlohmann/json.hpp 배치 (v3.11.3 다운로드)
[ ] 2. data/ 디렉토리 생성 및 빈 JSON 파일 3개 작성
[ ] 3. packages.config 삭제
[ ] 4. SampleOrderSystem.vcxproj 수정
      [ ] 4-1. gmock NuGet 참조 제거 (packages.config ItemGroup)
      [ ] 4-2. gmock.targets Import 제거
      [ ] 4-3. EnsureNuGetPackageBuildImports 타겟 제거
      [ ] 4-4. 4개 구성에 /utf-8 옵션 추가
      [ ] 4-5. 4개 구성에 AdditionalIncludeDirectories 추가
      [ ] 4-6. CopyDataDir post-build 타겟 추가
[ ] 5. main.cpp 교체 (UTF-8 설정 + 골격 주석)
[ ] 6. 빌드 및 실행으로 완료 기준 7가지 검증
```
