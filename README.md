# Mini Lexical Search

로컬 문서(.txt / .md)를 위한 **Line-aware Inverted Index** 기반 전문 검색 엔진.

매 검색마다 모든 파일을 읽는 대신, 문서를 인덱싱하여 토큰의 **등장 횟수(TF)** 와 **출처 줄 번호** 를 저장한다. 현재 검색은 Index로 관련 문서를 찾고, 모든 매칭 문서만 순차 Scan하여 모든 매칭 줄을 보여 준다. Ranking 정렬 및 Top-K는 후속 단계이다.

## 핵심 아이디어

```text
Documents → Tokenization → Line-aware Inverted Index
                                    │
Query → Index Lookup → Ranking → Heap Top-K → Matching Lines
```

* **Index**: `term → (doc, termFrequency, lines)`
* **Ranking (P0)**: `score = termFrequency`
* **Top-K**: Min Heap으로 `O(N log K)` 선택
* **매칭 줄**: 현재 모든 매칭 문서의 모든 매칭 줄 출력. 내부 최대 출력 개수 인자는 문서당 기본 `0`(무제한), 양수이면 앞 N개 줄 제한

`/search alpha`, `/search alpha AND beta`, `/search alpha OR beta`를 지원한다. 결과는 점수순이 아닌 문서 ID순이며 점수는 TF 합이다. 연산자는 독립된 대문자만 인식하고 혼합 AND/OR·괄호·따옴표·암묵적 연산자는 지원하지 않는다. 원문 변경은 `/index`로 갱신해야 하며 파일 읽기 실패는 문서별 경고로 표시한다. 위 Ranking/Top-K 흐름은 최종 목표이다.

## 예시

```text
> /search 자료구조

1. data_structure.md       score: 8.72
   12: 자료구조는 데이터를 효율적으로 저장하는 방법이다.
   40: 배열과 자료구조를 비교하면 다음과 같다.
```

## 빌드 및 실행

```bash
cmake -S . -B build
cmake --build build -j
./build/mini_lexical_search start
```

테스트:

```bash
ctest --test-dir build --output-on-failure
```

자세한 내용은 [개발 환경](docs/DEVELOPMENT.md)을 참고한다.

앱 실행 후 표준 입력으로 `/index [디렉터리]`, `/search <검색어>`, `/help`, `/exit` 또는 `/quit` 명령을 전달한다. `/index`는 기본 `./documents` 또는 지정한 디렉터리의 `.txt`·`.md` 문서 메타데이터를 `.data/index.json`에 저장한다. 각 문서에는 원문 바이트의 SHA-256 `hash`를 저장하고, 이전 목록과 비교해 `n not updated, n updated, n added, n deleted`를 출력한다. 변경이 없으면 JSON을 다시 쓰지 않는다. 이어서 추가·변경된 문서만 줄 단위로 토큰화하여 정규화 토큰 수를 `tokenCount`에 기록하고, 토큰·등장 횟수·줄 번호를 Line-aware Inverted Index에 반영한 뒤 `tokenized <문서 수> documents (<시간> ms)`로 소요 시간을 출력한다. 변경 없는 문서는 토큰화와 Posting 갱신을 건너뛴다. 앱은 시작할 때 스냅샷을 로드하므로 재실행 후에도 인덱스를 그대로 사용하며, 손상된 스냅샷은 덮어쓰지 않고 오류로 보고한다. 검색도 손상된 스냅샷 로드 실패 시 거부한다.

`-h`, `--help`, `-v`, `--version`은 실행 시 옵션으로 사용할 수 있다.

저장 위치는 현재 작업 디렉터리 기준이다. 저장 시 `.data/`가 없으면 생성을 시도하므로 해당 경로에 대한 쓰기 권한이 필요하다.

## 문서

* [PRD](docs/PRD.md)
* [개발 환경](docs/DEVELOPMENT.md)

## 개발 단계

* **P0** — TXT/Markdown 인덱싱, 단일 키워드 검색, TF Ranking, Top-K, 매칭 줄 출력
* **P1** — AND/OR 검색, TF-IDF/BM25, Trie 자동완성, 한국어 형태소 분석(Kiwi)
* **P2** — Hybrid / ANN Search / PDF 인덱싱
* **P3** — Semantic Search (Embedding)

## 기술 스택

ISO C++20 (`-std=c++20`), CMake, STL (SHA-256 자체 구현)
