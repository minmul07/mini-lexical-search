# Mini Lexical Search

로컬 문서(.txt / .md)를 위한 **Line-aware Inverted Index** 기반 전문 검색 엔진.

매 검색마다 모든 파일을 읽는 대신, 문서를 인덱싱하여 토큰의 **등장 횟수(TF)** 와 **출처 줄 번호** 를 저장한다. 검색 시에는 Index로 관련 문서를 찾고, Top-K 문서만 다시 순차 Scan하여 grep처럼 매칭 줄을 보여 준다.

## 핵심 아이디어

```text
Documents → Tokenization → Line-aware Inverted Index
                                    │
Query → Index Lookup → Ranking → Heap Top-K → Matching Lines
```

* **Index**: `term → (doc, termFrequency, lines)`
* **Ranking (P0)**: `score = termFrequency`
* **Top-K**: Min Heap으로 `O(N log K)` 선택
* **매칭 줄**: Top-K 파일만 순차 Scan하여 줄 원문 추출, 문서당 최대 3줄

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

앱 실행 후 표준 입력으로 `/index [디렉터리]`, `/search <검색어>`, `/help`, `/exit` 또는 `/quit` 명령을 전달한다. `/index`는 기본 `./documents` 또는 지정한 디렉터리의 `.txt`·`.md` 문서 메타데이터를 `.data/documents.json`에 저장한다. `tokenCount`는 `0`으로 유지하며, 토큰화·검색은 아직 미구현이다.

`-h`, `--help`, `-v`, `--version`은 실행 시 옵션으로 사용할 수 있다.

저장 위치는 현재 작업 디렉터리 기준이다. 저장 시 `.data/`가 없으면 생성을 시도하므로 해당 경로에 대한 쓰기 권한이 필요하다. 역색인 저장 및 JSON 로드는 추후 구현한다.

## 문서

* [PRD](docs/PRD.md)
* [개발 환경](docs/DEVELOPMENT.md)

## 개발 단계

* **P0** — TXT/Markdown 인덱싱, 단일 키워드 검색, TF Ranking, Top-K, 매칭 줄 출력
* **P1** — AND/OR 검색, TF-IDF/BM25, Trie 자동완성, 한국어 형태소 분석(Kiwi)
* **P2** — Hybrid / ANN Search / PDF 인덱싱
* **P3** — Semantic Search (Embedding)

## 기술 스택

C++17 이상, CMake, STL
