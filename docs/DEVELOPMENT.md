# 개발 가이드

## 환경과 제약

| 항목 | 기준 |
| --- | --- |
| 채점 환경 | Ubuntu 24.04 LTS 64-bit / GNU g++ |
| 언어 | ISO C++20 (`-std=c++20`) |
| 빌드 도구 | CMake 3.20 이상 |
| 외부 라이브러리 | FTXUI만 허용하며 UI에만 사용. 핵심 로직은 STL과 직접 작성한 코드로 구현 |
| 제출 조건 | FTXUI 소스 포함, 전체 10MB 이하, 설치·다운로드 없이 `make` 한 번으로 빌드 |

현재는 CMake 빌드를 지원하며, 제출용 `make` 단일 명령 빌드는 아직 구현되지 않았다.
FTXUI 7.0.3 소스는 `third_party/ftxui/`에 포함되어 있다.
상세 조건은 [FTXUI 사용 승인](FTXUI_APPROVAL.md)을 참고한다.

## 빌드와 검증

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

| 옵션 | 기본값 | 용도 |
| --- | --- | --- |
| `CMAKE_BUILD_TYPE` | `Debug` | 최적화 빌드는 `Release`로 지정 |
| `BUILD_TESTING` | `ON` | `OFF`이면 테스트 빌드 제외 |

빌드 산출물은 Git에서 제외되는 `build/`에 생성된다.
분야별 검증 항목은 [테스트 가이드](TESTING.md)를 참고한다.

## 실행과 명령

```bash
./build/mini_lexical_search start           # TUI
./build/mini_lexical_search start --no-tui  # CLI REPL
```

| 명령 | 동작 |
| --- | --- |
| `/index [디렉터리]` | 문서 수집 및 인덱스 갱신. 기본 경로는 `./documents` |
| `<검색어>` | `/` 없이 바로 입력하여 단일 키워드 또는 대문자 AND/OR 검색 |
| `/next`, `/prev` | TUI의 다음·이전 파일 페이지 |
| `/more <상대 경로>` | TUI 현재 페이지 파일의 다음 최대 10행 펼치기 |
| `/help` | 명령 도움말 |
| `/exit`, `/quit` | 종료 |

실행 옵션은 `-h`·`--help`, `-v`·`--version`, `start`와 함께 사용하는 `--no-tui`다.
CLI는 EOF로도 종료하며, TUI는 Esc·Ctrl+C로도 종료한다.
TUI와 CLI는 같은 입력 처리를 공유한다. 빈 입력은 무시하고, `/`로 시작하면 명령으로 처리한다. `/search` 명령은 제거되었으며 알 수 없는 명령으로 처리한다. 화면과 입력 규칙은 [TUI 디자인](TUI_DESIGN.md)을 따른다.

## 구조와 역할

공개 헤더는 `include/mini_lexical_search/` 아래에 소스와 같은 계층으로 배치한다.

| 위치 | 핵심 역할 |
| --- | --- |
| `src/main.cpp` | 실행 인자 처리, 앱 구성, TUI·CLI 선택 |
| `src/domain/` | 데이터 모델, 토큰화, 역색인, 질의 처리, Top-K |
| `src/application/` | `SearchApplication`의 인덱스 상태, 인덱싱·검색 절차, 진행 이벤트·로그 |
| `src/infrastructure/` | 문서 수집·읽기, SHA-256, JSON·바이너리 저장과 로드 |
| `src/presentation/cli/` | 공통 명령 처리와 CLI 입출력 |
| `src/presentation/tui/` | FTXUI 화면·입력, CLI 명령 처리 호출 |
| `tests/` | CTest 기반 자동 테스트 |
| `benchmarks/` | 수동 인덱스·검색 성능 측정 |

Presentation은 Application을 사용하고, Application은 Infrastructure와 Domain을 사용한다.
Infrastructure는 Domain에 의존하며, Domain은 다른 계층에 의존하지 않는다.
Application은 UI 타입·콘솔 출력에 의존하지 않고, FTXUI는 TUI 타깃에만 연결한다.

공통 API는 `application/search_application.hpp`의 `SearchApplication`이다.
`index`는 인덱싱과 단계별 진행 이벤트를, `search`는 첫 `SearchPage`를 반환한다.
`currentPage`, `nextPage`, `previousPage`, `expandFile`은 파일 그룹·전체 건수·읽은 행·경고와 페이지 상태를 제공한다.
`visitAllResults`는 파일 시작·매칭 행·읽기 경고 콜백으로 전체 결과를 전달하며 원문을 캐시하지 않는다. 콜백 예외는 호출자에게 전달한다.
`clearSearch` 또는 새 검색·인덱싱 시작은 세션과 Posting 참조를 해제한다. 세션은 Application이 단독 소유한다.
문서 ID만 먼저 계산·정렬하고, 방문한 파일의 Posting 행을 커서로 병합한다. 전체 후보의 매칭 행 벡터를 복사하지 않는다.
파일별 전체 행 수를 계산한 뒤 표시할 원문만 읽고, 펼치기는 직전 바이트 위치에서 이어 읽는다. 방문한 페이지의 원문과 펼친 상태는 세션 동안 캐시한다.
기존 `QueryProcessor::process`와 Top-K는 독립 알고리즘·테스트용으로 유지하며 실제 UI 검색 경로는 `matchingDocuments`를 사용한다.
진행·검색 콜백에서 앱을 재호출하거나 여러 스레드에서 동시에 접근하지 않는다.

## 핵심 동작과 주의 사항

| 분야 | 핵심 규칙 |
| --- | --- |
| 문서 수집 | 하위 디렉터리의 소문자 `.txt`·`.md` 일반 파일만 수집하며 심볼릭 링크는 제외 |
| 변경 감지 | 경로·SHA-256·제목으로 비교. 기존 ID를 유지하고 추가·변경 문서만 토큰화 |
| 동기화 | `/index`의 입력 디렉터리를 기준으로 전체 목록을 동기화. 파일 이동은 삭제와 추가로 처리 |
| 토큰화 | ASCII 영숫자와 한글을 유지하고 영문은 소문자로 변환. 나머지 문자와 잘못된 UTF-8은 구분자 |
| 검색 | 단일 키워드·AND·OR 지원. 혼합 AND/OR, 괄호, 따옴표, 암묵적 연산자는 미지원 |
| 순서와 출력 | 디렉터리 우선·UTF-8 이름순, 파일 내 줄 번호순. TUI 파일당 초기 10행·50행 사전 확인, no-TUI 전체 출력 |
| 원문 읽기 | 선정된 문서만 읽으며 실패는 문서별 경고로 반환. 원문 변경은 `/index`로 갱신 필요 |
| 저장 | 현재 작업 디렉터리의 `.data/index.bin` 사용. 변경이 없으면 재저장하지 않고 실패 시 기존 파일·메모리 상태 보존 |
| 시작과 복구 | 스냅샷이 없으면 빈 인덱스로 시작. 손상되면 재생성 여부를 묻고, 거절하면 파일을 유지한 채 종료 |
| 파일 형식 | 인덱싱 루트를 포함하는 version 4의 Varint + Delta 형식만 지원. v1~v3은 재구축 필요 |
| 로그 | `.data/log.log`에 로드·갱신·검색 이벤트 기록. 검색 요약에는 문서 ID·전체 매칭 행 수를 남기고, 탐색 기록에는 명령·경로·페이지를 남긴다. 원문은 제외하며 로그 실패는 명령 결과에 영향 없음 |

`.data/`에 쓰기 권한이 필요하다. 강제 종료 후 `index.bin.tmp` 디렉터리가 남으면
실행 중인 저장 작업이 없는지 확인하고 제거한 뒤 재시도한다.
TUI는 PgUp/PgDn으로 출력 영역을 스크롤한다. 펼치기와 페이지 이동의 상세 계약은 [TUI 디자인](TUI_DESIGN.md)을 따른다.
