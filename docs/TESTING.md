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

## 자동 테스트

| 분야 | 테스트 | 핵심 확인 항목 |
| --- | --- | --- |
| 버전 | `version_test` | 버전 문자열 |
| 토큰화 | `tokenizer_test` | 영숫자·한글 유지, 소문자 변환, 구분자·잘못된 UTF-8 처리 |
| 해시 | `sha256_test` | 표준 벡터, 패딩 경계, 청크 분할과 일괄 해시의 일치 |
| 문서 처리 | `document_test` | 수집·필터링, JSON 저장·로드, 변경 감지·ID 보존, 변경 문서만 토큰화 |
| 역색인 | `inverted_index_test` | TF·줄 번호·문서 정렬, 추가·삭제·병합, 불변식 및 실패 시 상태 보존 |
| 질의 처리 | `query_processor_test` | 단일·AND·OR 검색, TF 합산·중복 제거, 문법 오류, 원본 Posting 보존 |
| 순위 선정 | `top_k_test` | 점수·동점 순위, K 경계값, 전체 정렬 결과와의 일치 |
| 매칭 행 | `matching_lines_test` | 원본 줄 번호·줄바꿈, 출력 한도, 잘못된 줄 목록·읽기 실패 |
| 인덱싱·저장 | `indexer_test` | 스냅샷 왕복·형식 검증, 증분 갱신, 변경 없는 파일 보존, 저장 실패 복구 |
| 공통 Application | `search_application_test` | 인덱싱·재로드·검색, 진행 이벤트, Top-K·매칭 행, 오류 복구·상태 라벨 |
| CLI 통합 | `cli_test` | 실행 옵션·명령, 실제 저장·재실행, 검색 순위·시간 출력·로그, 손상 스냅샷·입력 오류 처리 |

## 수동 확인

| 분야 | 실행 방법 | 핵심 확인 항목 |
| --- | --- | --- |
| CLI 대화형 입력 | `./build/mini_lexical_search start --no-tui` | `/index`, 단일·AND·OR `/search`, `/help`, `/exit`의 입력·출력과 종료 |
| TUI 화면·입력 | `./build/mini_lexical_search start` | 입력 상자 줄바꿈, Enter 실행·초기화, 출력 교체, 상태 라벨, 종료 키 |
| TUI 손상 복구 | 손상 스냅샷이 있는 작업 디렉터리에서 TUI 실행 | `reset index`·`quit` 팝업의 키보드·마우스 선택과 복구·종료 |

TUI 화면은 CTest로 검증하지 않는다. 화면 기준은 [TUI 디자인](TUI_DESIGN.md)을 참고한다.
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
