# 테스트 가이드

## 실행

코드 변경 시 관련 분야를 확인한다. CLI·명령 처리 또는 공통 Application 변경 시 전체 자동 테스트를 실행한다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

문서만 변경한 경우 내용의 정확성, 링크, diff 형식을 확인한다.
자동 저장 테스트는 빌드 디렉터리의 격리된 경로를 사용한다.

제출용 Makefile 변경 시 `make clean` 후 `make`만으로 빌드하고 루트 실행 파일의 `--help`, `--version`과 격리된 작업 디렉터리에서 no-TUI 인덱싱·검색을 확인한다. 재실행 시 불필요한 빌드가 없는지, 헤더 변경 시 관련 소스가 다시 컴파일되는지, `make clean`이 기존 CMake 산출물을 보존하는지도 확인한다. Makefile은 CMake 호출·설치·다운로드 없이 빌드해야 한다.

## 자동 테스트

| 분야 | 테스트 | 핵심 확인 항목 |
| --- | --- | --- |
| 버전 | `version_test` | 버전 문자열 |
| 토큰화 | `tokenizer_test` | 영숫자·한글 유지, 소문자 변환, 구분자·잘못된 UTF-8 처리·원문 토큰 바이트 범위 |
| 해시 | `sha256_test` | 표준 벡터, 패딩 경계, 청크 분할과 일괄 해시의 일치 |
| 문서 처리 | `document_test` | 수집·필터링, JSON 저장·로드, 변경 감지·ID 보존, 변경 문서만 토큰화 |
| 역색인 | `inverted_index_test` | TF·줄 번호·문서 정렬, 추가·삭제·병합, 불변식 및 실패 시 상태 보존 |
| 질의 처리 | `query_processor_test` | 단일·AND·OR 검색, TF 합산·중복 제거, 문법 오류, 원본 Posting 보존 |
| 순위 선정 | `top_k_test` | 점수·동점 순위, K 경계값, 전체 정렬 결과와의 일치 |
| 매칭 행 | `matching_lines_test` | 원본 줄 번호·줄바꿈, 출력 한도, 잘못된 줄 목록·읽기 실패 |
| 인덱싱·저장 | `indexer_test` | v4 루트 왕복·구버전 거부·루트 변경·형식 검증, 증분 갱신, 변경 없는 파일 보존, 저장 실패 복구 |
| 공통 Application | `search_application_test` | 인덱싱·재로드·검색, 진행 이벤트, 파일 트리순·중복 제거·페이지·펼치기, 오류 복구·세션 무효화·상태 라벨, 뷰어 전체 원문 읽기·공백 경로·삭제/변경·검색 세션 보존 |
| 검색 명령 | `search_commands_test` | TUI/no-TUI의 일반 텍스트·AND/OR 검색, 빈 입력·제거된 검색·탐색 명령 거부, 출력 형식·파일 건수·버튼 탐색·공백 경로·경계·읽기 경고·구조화된 피드백 |
| TUI 결과 모델 | `result_view_test` | 1~3줄 발췌·셀 너비·토큰 강조·한글/이모지/결합 문자, 행 선택·스크롤·리사이즈·더보기 확장 시 스크롤 위치 유지·홈 초기화 |
| TUI 뷰어 | `viewer_view_test` | 진입 줄 배치·파일 경계, 키보드·휠·Ctrl+휠, 기본 줄번호·줄바꿈 전환, 드래그 선택·줄번호 제외 복사 문자열·전각 글리프, 가로 스크롤·셀 너비·글리프 보존, 리사이즈·원본 위치 유지·빈 파일·오류 |
| TUI 클립보드 | `clipboard_test` | Base64 표준 벡터, OSC 52 시퀀스 형식·출력 |
| TUI 입력 | `tui_input_test` | 조합 이모지의 좌우·상하 이동·삭제·마우스 편집, 반복 입력·리사이즈·줄바꿈 커서 보존, 출력 셀·커서·테두리 정렬 |
| TUI 인덱싱 진행 | `tui_index_test` | 진행률 단계 전환·로그·완료/실패 상태, 검색창 위치 고정, 셀 폭 기준 fill과 텍스트 보존 |
| CLI 통합 | `cli_test` | 실행 옵션·명령, 실제 저장·재실행, 파일별 전체 출력·시간·로그, 손상 스냅샷·입력 오류 처리 |

## 수동 확인

| 분야 | 실행 방법 | 핵심 확인 항목 |
| --- | --- | --- |
| CLI 대화형 입력 | `./build/mini_lexical_search start --no-tui` | `/index`, 단일·AND·OR 검색어, `/help`, `/exit`의 입력·출력과 종료 |
| TUI 화면·입력 | `./build/mini_lexical_search start` | 전체 배경 검정, 상단 검색창·실행 검색어 고정, 최대 3줄 입력·발췌·강조, ↑↓로 검색창 마지막 줄↔첫 결과 이동·여러 줄 입력 커서 이동, 조합 이모지 반복 입력·좌우 이동·삭제·리사이즈 후 커서와 테두리 정렬, Tab 무동작, Home/End 결과 선택, 행 한 번 클릭은 선택만·더블클릭은 뷰어 진입, 버튼 클릭·Space·Enter 실행, 더보기 후 위쪽 행 위치 유지·200ms ease-out 순차 확장·아래 항목이 함께 밀려 내려가며 공간 확보·애니메이션 중 입력 시 즉시 완료, 검색창 Space 공백·Enter 검색, 휠·PgUp/PgDn, 고정 페이지 버튼, 리사이즈 직후 스크롤 |
| TUI 홈·종료 | 격리된 경로에서 검색 후 Esc 및 재검색 | 홈 복귀 시 입력·페이지·펼침 초기화와 인덱스 유지, 홈 Esc 무동작, `/help` 후 탐색, Ctrl+C·`/exit`·`/quit` 종료와 터미널 복구 |
| TUI 뷰어 | 격리된 경로에서 검색 후 행 더블클릭 또는 선택 후 Space·Enter | 검색 줄이 본문 위쪽 5줄 이내·중앙 이하로 내려가지 않게 표시, 고정 파일명·단축키 안내, 줄번호 기본 표시·n 전환·w 줄바꿈, ↑↓·PgUp/PgDn·Home/End·휠, 본문 드래그 선택·우클릭 복사(OSC 52 지원 터미널)·우측 상단 복사 알림(300ms 슬라이드·3초 유지)·스크롤 시 선택 해제, 줄바꿈 해제 후 ←→·Ctrl+휠, 리사이즈, Esc 후 검색어·커서·선택·스크롤·페이지·펼침 복원, 파일 삭제·단축 후 오류/안내 및 복귀 |
| TUI 인덱싱 | 문서가 많은 격리된 경로에서 `/index` 실행 | 검색창이 위로 이동하지 않고 내부 fill이 왼쪽에서 오른쪽으로 차오름, 단계별 로그가 검색창 아래에 고정, 인덱싱 중 Enter 무시와 Ctrl+C 종료 지연, 완료 후 검색 |
| TUI 손상 복구 | 손상 스냅샷이 있는 작업 디렉터리에서 TUI 실행 | `reset index`·`quit` 팝업의 키보드·마우스 선택과 복구·종료, Esc 무동작, 재생성 중 fill·실패 시 팝업 재표시 |

TUI 발췌·결과 모델은 CTest로 검증하고, 실제 화면 렌더링·터미널 이벤트는 수동 또는 PTY로 검증한다. 화면 기준은 [TUI 디자인](TUI_DESIGN.md)을 참고한다.
수동 인덱싱은 현재 작업 디렉터리의 `.data/index.bin`을 갱신하므로 격리된 경로에서 확인한다.

## 성능 측정

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target index_benchmark -j 4
./build/release/index_benchmark documents build/index-benchmark-new.bin
```

| 분야 | 핵심 확인 항목 |
| --- | --- |
| 바이너리 인덱스 | 저장 포함 전체 인덱싱·별도 재저장·로드 시간, 파일 크기, 반복 간 Posting 일치 |

출력 파일은 존재하지 않아야 한다. 워밍업 1회 후 7회 측정하며 마지막 스냅샷은 보존한다.
CSV의 `index_and_save_ms`는 문서 수집·해시·토큰화·역색인 생성과 최초 저장을 포함한다.
`save_ms`는 같은 메모리 스냅샷을 별도로 재저장한 시간이고, `load_ms`는 저장 파일을 로드한 시간이다.
세 시간은 독립된 측정값이므로 전체 인덱싱 시간을 구하려고 합산하지 않는다.
결과·환경·한계는 [성능 측정 기록](../.backlog/binary_indexing.md)에 기록한다.

## 검색 성능 측정

격리된 작업 디렉터리에서 CLI `/index <문서 디렉터리>`로 v4 스냅샷을 먼저 만든다.

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target search_benchmark -j 4
./build/release/search_benchmark /tmp/search-fixture/.data/index.bin hit
```

기존 스냅샷만 읽는다. 첫 페이지·파일 10행 펼치기·후속 페이지·전체 순회 시간을 밀리초로 측정하며, 해당 동작이 없으면 `-1`이다. 전체 순회는 출력 콜백에서 행 수만 세므로 실제 터미널 출력 시간은 제외한다. Linux `/proc/self/status`의 VmHWM으로 로드 후·첫 페이지 후·전체 순회 후 프로세스 최대 RSS(KiB)를 기록한다. 로더의 임시 메모리도 포함하므로 검색 전용 메모리 증가량으로 해석하지 않는다.

흔한 검색어·희귀 검색어·다수 작은 파일·긴 단일 파일에서 실행하며 결과는 [검색 성능 기록](SEARCH_BENCHMARK.md)에 남긴다.
