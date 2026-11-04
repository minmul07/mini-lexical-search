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

`/search alpha`, `/search alpha AND beta`, `/search alpha OR beta`를 지원한다. 연산자는 독립된 대문자만 인식하고 혼합 AND/OR·괄호·따옴표·암묵적 연산자는 지원하지 않는다. AND는 문서 기준으로 판정하며, 각 검색어가 일치한 행의 합집합을 중복 없이 출력한다.

파일 제목은 인덱싱 루트 기준 상대 경로이며, 건수는 파일 전체 매칭 행이 1~10개면 실제 숫자, 11개 이상이면 `10+`다. TUI의 ` > N more`는 아직 펼치지 않은 행 수다. `/more <상대 경로>`로 다음 최대 10행을 추가하고 `/next`, `/prev`로 파일 페이지를 이동한다. 펼친 상태와 페이지 경계는 유지한다. 공백이 있는 경로는 따옴표 없이 명령 뒤에 그대로 입력한다. PgUp/PgDn으로 출력 영역을 스크롤한다.

원문 변경은 `/index`로 갱신해야 한다. 읽기 실패는 파일별 경고로 표시하고 다른 파일을 계속 처리한다. 실패한 파일은 새 검색에서 재시도한다. 새 검색 시도 또는 인덱싱·재생성 시작 시 이전 검색 세션을 지운다.

## 예시

```text
> /search 자료구조
searched (1.234 ms)
=============================
[data_structure.md] 2
12: 자료구조는 데이터를 효율적으로 저장하는 방법이다.
40: 배열과 자료구조를 비교하면 다음과 같다.
=============================
```

## 빌드 및 실행

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

`start`는 기본적으로 FTXUI TUI를 연다. 터미널 중앙의 최대 너비 100열 흰색 테두리 입력 상자에서 Enter로 `/index`, `/search`, `/help`, `/exit`, `/quit`를 실행하고, 출력은 상자 아래에 표시한다. `/exit`, `/quit`, Esc, Ctrl+C로 종료한다. 같은 명령의 줄 단위 REPL은 `--no-tui`로 실행한다. CLI 실행 후 표준 입력으로 `/index [디렉터리]`, `/search <검색어>`, `/help`, `/exit` 또는 `/quit` 명령을 전달한다. `/index`는 기본 `./documents` 또는 지정한 디렉터리의 `.txt`·`.md` 문서 메타데이터와 역색인을 `.data/index.bin` 바이너리 스냅샷에 저장한다. 각 문서에는 원문 바이트의 SHA-256 `hash`를 저장하고, 이전 목록과 비교해 `n not updated, n updated, n added, n deleted`를 출력한다. 변경이 없으면 스냅샷을 다시 쓰지 않는다. 이어서 추가·변경된 문서만 줄 단위로 토큰화하여 정규화 토큰 수를 `tokenCount`에 기록하고, 토큰·등장 횟수·줄 번호를 Line-aware Inverted Index에 반영한 뒤 `tokenized <문서 수> documents (<시간> ms)`로 소요 시간을 출력한다. 변경 없는 문서는 토큰화와 Posting 갱신을 건너뛴다. 앱은 시작할 때 스냅샷을 로드하므로 재실행 후에도 인덱스를 그대로 사용하며, 손상된 스냅샷은 덮어쓰지 않고 오류로 보고한다. 검색도 손상된 스냅샷 로드 실패 시 거부한다. `/search`는 결과 앞에 `searched (0.123 ms)`처럼 시간을 소수점 셋째 자리 밀리초로 출력한다. TUI는 첫 페이지 구성까지, no-TUI는 후보 준비와 첫 파일의 건수 계산까지 측정하며 이후 전체 원문 출력 시간은 제외한다. `/index`의 메타데이터·토큰화 시간도 같은 형식으로 출력한다. 로드, 인덱스 갱신, 검색 기록은 `.data/log.log`에 이벤트당 한 줄로 추가한다.

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

ISO C++20 (`-std=c++20`), CMake, STL (SHA-256 자체 구현)
