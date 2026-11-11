# Mini Lexical Search

로컬 문서(.txt / .md)를 위한 **Line-aware Inverted Index** 기반 전문 검색 엔진.

매 검색마다 모든 파일을 읽는 대신, 문서를 인덱싱하여 토큰의 **등장 횟수(TF)** 와 **출처 줄 번호** 를 저장한다. 검색은 Index로 일치 문서를 찾고, 디렉터리 우선·이름순으로 파일을 묶어 매칭 줄을 보여 준다.

## 핵심 아이디어

```text
Documents → Tokenization → Line-aware Inverted Index
                                    │
Query → Matching Document IDs → Directory/File Order → Matching Lines
```

* **Index**: `term → (doc, termFrequency, lines)`
* **정렬**: 각 디렉터리에서 하위 디렉터리 우선, 이름은 UTF-8 바이트순. 파일 안에서는 1-based 줄 번호순
* **TUI**: 파일마다 처음 최대 10행. 파일 추가 전 누적 행이 50 미만인지 확인하며, 파일별 펼치기는 50행을 넘어도 허용
* **no-TUI**: 일치하는 모든 파일·행을 한 번에 출력

`alpha`, `alpha AND beta`, `alpha OR beta`처럼 `/` 없이 입력하면 바로 검색한다. 빈 입력은 무시하고 `/`로 시작하는 입력은 명령으로 처리한다. `/search` 명령은 사용하지 않는다. 연산자는 독립된 대문자만 인식하고 혼합 AND/OR·괄호·따옴표·암묵적 연산자는 지원하지 않는다. AND는 문서 기준으로 판정하며, 각 검색어가 일치한 행의 합집합을 중복 없이 출력한다.

파일 제목은 인덱싱 루트 기준 상대 경로이며, 건수는 파일 전체 매칭 행이 1~10개면 실제 숫자, 11개 이상이면 `10+`다. TUI의 `더보기 · N행 남음` 버튼은 아직 펼치지 않은 행 수를 표시한다. 클릭 또는 키보드 선택 후 Space 또는 Enter로 다음 최대 10행을 펼친다. 이전·다음 페이지 버튼도 클릭 또는 키보드 선택 후 Space 또는 Enter로 실행한다. 더보기와 페이지 이동은 TUI 버튼으로만 조작하며 펼친 상태와 페이지 경계는 유지한다. 버튼은 경로의 공백을 그대로 보존한다. 검색창 마지막 줄에서 ↓로 첫 결과에 진입하고, 첫 결과에서 ↑로 검색창으로 돌아온다. 결과 안에서는 ↑↓로 행·버튼을 선택한다. 휠·PgUp/PgDn으로 결과를 스크롤한다.

원문 변경은 `/index`로 갱신해야 한다. 읽기 실패는 파일별 경고로 표시하고 다른 파일을 계속 처리한다. 실패한 파일은 새 검색에서 재시도한다. 새 검색 시도 또는 인덱싱·재생성 시작 시 이전 검색 세션을 지운다.

## no-TUI 출력 예시

```text
> 자료구조
searched (1.234 ms)
=============================
[data_structure.md] 2
12: 자료구조는 데이터를 효율적으로 저장하는 방법이다.
40: 배열과 자료구조를 비교하면 다음과 같다.
=============================
```

## 빌드 및 실행

제출 환경에서는 별도 설치·다운로드나 CMake 없이 빌드한다.

```bash
make
./mini_lexical_search start
./mini_lexical_search start --no-tui
```

개발 및 자동 테스트용 CMake 빌드:

```bash
cmake -S . -B build
cmake --build build -j
./build/mini_lexical_search start
./build/mini_lexical_search start --no-tui
```

테스트:

```bash
ctest --test-dir build --output-on-failure
```

자세한 내용은 [개발 환경](docs/DEVELOPMENT.md)을 참고한다.

`start`는 기본적으로 FTXUI TUI를 연다. 홈의 중앙 입력 상자에서 검색어 또는 `/index`, `/help`, `/exit`, `/quit` 명령을 입력하고 Enter로 실행한다. `/index`는 홈 검색창 위치를 유지한 채 별도 스레드에서 실행되며, 검색창 내부가 진행률만큼 왼쪽에서 오른쪽으로 채워지고 단계별 로그가 검색창 아래에 표시된다. 검색 후에는 좌측 정렬의 전체 너비 화면으로 전환하고 검색창·실행 검색어와 하단 페이지 버튼을 고정한다. 파일 사이에는 수평선을 두며, 긴 원문은 첫 일치 주변을 최대 3줄로 발췌하고 검색 토큰을 강조한다. Esc는 검색 상태를 지우고 홈으로 돌아가며 종료하지 않는다. 홈에서 Esc는 아무 동작도 하지 않는다. `/exit`, `/quit`, Ctrl+C로 종료한다. 같은 명령의 줄 단위 REPL은 `--no-tui`로 실행한다. CLI 실행 후 표준 입력으로 `/index [디렉터리]`, `/help`, `/exit` 또는 `/quit` 명령과 검색어를 전달한다. `/index`는 기본 `./documents` 또는 지정한 디렉터리의 `.txt`·`.md` 문서 메타데이터와 역색인을 `.data/index.bin` 바이너리 스냅샷에 저장한다. 각 문서에는 원문 바이트의 SHA-256 `hash`를 저장하고, 이전 목록과 비교해 `n not updated, n updated, n added, n deleted`를 출력한다. 변경이 없으면 스냅샷을 다시 쓰지 않는다. 이어서 추가·변경된 문서만 줄 단위로 토큰화하여 정규화 토큰 수를 `tokenCount`에 기록하고, 토큰·등장 횟수·줄 번호를 Line-aware Inverted Index에 반영한 뒤 `tokenized <문서 수> documents (<시간> ms)`로 소요 시간을 출력한다. 변경 없는 문서는 토큰화와 Posting 갱신을 건너뛴다. 앱은 시작할 때 스냅샷을 로드하므로 재실행 후에도 인덱스를 그대로 사용하며, 손상된 스냅샷은 덮어쓰지 않고 오류로 보고한다. 검색도 손상된 스냅샷 로드 실패 시 거부한다. 검색 시간은 소수점 셋째 자리 밀리초로 표시하며, no-TUI에서는 결과 앞에 `searched (0.123 ms)`로 출력한다. TUI는 첫 페이지 구성까지, no-TUI는 후보 준비와 첫 파일의 건수 계산까지 측정하며 이후 전체 원문 출력 시간은 제외한다. `/index`의 메타데이터·토큰화 시간도 같은 형식으로 출력한다. 로드, 인덱스 갱신, 검색 기록은 `.data/log.log`에 이벤트당 한 줄로 추가한다.

스냅샷은 인덱싱 루트를 포함하는 v4 형식이다. v1~v3은 재구축해야 하며 시작 시 재생성을 선택하거나 기존 파일을 백업·제거한 뒤 `/index`를 실행한다.

`-h`, `--help`, `-v`, `--version`, `--no-tui`는 실행 시 옵션으로 사용할 수 있다.

저장 위치는 현재 작업 디렉터리 기준이다. 저장 시 `.data/`가 없으면 생성을 시도하므로 해당 경로에 대한 쓰기 권한이 필요하다.

## 문서

* [PRD](docs/PRD.md)
* [개발 환경](docs/DEVELOPMENT.md)
* [FTXUI 사용 승인 및 적용 범위](docs/FTXUI_APPROVAL.md)
* [TUI 디자인](docs/TUI_DESIGN.md)

## 개발 단계

* **P0** — TXT/Markdown 인덱싱, 단일·AND/OR 검색, 파일별 매칭 줄 출력·펼치기·페이지 탐색
* **P1** — AND/OR 검색, TF-IDF/BM25, Trie 자동완성
* **P2** — Hybrid / ANN Search / PDF 인덱싱
* **P3** — Semantic Search (Embedding)

## 기술 스택

ISO C++20 (`-std=c++20`), GNU Make / CMake, STL (SHA-256 자체 구현)
