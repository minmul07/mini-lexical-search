# Mini Search Engine — PRD

## 1. 프로젝트 개요

### 프로젝트명

**Mini Search Engine**
로컬 문서에 대한 자료구조 기반 전문 검색(Full-text Search) 엔진

### 목적

다수의 텍스트 문서를 검색할 때 매 검색마다 모든 파일을 순차 탐색하는 대신, 문서를 사전에 **인덱싱(Indexing)** 하여 검색에 적합한 자료구조로 저장하고 빠르게 관련 문서를 검색하는 로컬 검색 엔진을 구현한다.

프로젝트의 핵심은 검색 기능 자체보다 다음과 같은 자료구조가 실제 검색 시스템에서 어떤 문제를 해결하는지를 구현을 통해 확인하는 것이다.

* Hash Table — 단어 기반 Inverted Index
* Vector / Posting List — 단어가 등장하는 문서 정보 저장
* Set — AND / OR 등 복합 질의 처리
* Heap / Priority Queue — 검색 결과 Top-K 선정
* Trie — 검색어 자동완성
* 선택적으로 Graph 기반 ANN 등 — Semantic Search 확장

---

# 2. 문제 정의

가장 단순한 문서 검색은 사용자가 검색할 때마다 모든 파일의 내용을 확인하는 방식으로 구현할 수 있다.

```text
Query
  ↓
doc1 전체 검색
doc2 전체 검색
doc3 전체 검색
...
docN 전체 검색
```

문서 수와 데이터 크기가 증가하면 매 검색마다 전체 데이터를 읽어야 하므로 검색 비용이 증가한다.

본 프로젝트에서는 문서를 최초 등록 또는 갱신할 때 한 번 분석하여 검색용 Index를 생성한다.

```text
Documents
    ↓
Tokenization / Normalization
    ↓
Indexing
    ↓
Inverted Index
```

검색 시에는 원본 파일 전체를 다시 읽지 않고 생성된 Index를 조회한다.

```text
Query: "자료구조"

       ↓

Hash Table Lookup

       ↓

자료구조
→ doc3
→ doc17
→ doc41

       ↓

Ranking

       ↓

Top-K results
```

---

# 3. 목표

## 3.1 핵심 목표

1. 다수의 로컬 문서를 사전에 인덱싱할 수 있다.
2. 특정 키워드가 포함된 문서를 Inverted Index를 통해 검색할 수 있다.
3. 검색 결과에 관련도 점수를 부여할 수 있다.
4. Heap을 이용해 관련도가 높은 상위 K개의 결과를 반환할 수 있다.
5. 여러 키워드에 대한 AND / OR 검색을 지원한다.
6. 자료구조 선택에 따른 검색 시간과 메모리 사용 특성을 비교할 수 있다.

## 3.2 확장 목표(낮은 우선순위)

* Trie 기반 검색어 자동완성
* 한국어 형태소 분석
* TF-IDF / BM25 기반 Ranking
* Semantic Search
* Lexical + Semantic Hybrid Search

---

# 4. 대상 사용자

본 프로그램은 대규모 웹 검색을 목적으로 하지 않는다.

주 사용 사례는 사용자의 컴퓨터에 존재하는 비교적 제한된 규모의 문서를 빠르게 검색하는 것이다.

예:

```text
documents/
├── data_structure.md
├── operating_system.txt
├── android_notes.md
├── algorithm.md
└── ...
```

사용자가 다음과 같이 검색한다.

```text
> search 자료구조
```

출력:

```text
1. data_structure.md       score: 8.72
2. algorithm.md            score: 5.31
3. cpp_notes.md            score: 3.98
```
TODO
---

# 5. 지원 데이터

## P0

지원 파일:

* `.txt`
* `.md`

문자 인코딩:

* UTF-8

초기 구현에서는 파일 형식을 제한하여 문서 Parser 구현보다 자료구조와 검색 알고리즘 구현에 집중한다.

## 이후 확장

* PDF - P1
* HTML - P2
* DOCX - P1

등은 별도의 Text Extractor를 통해 동일한 Indexing Pipeline에 입력할 수 있다.

---

# 6. 전체 시스템 구조

```text
                  ┌──────────────────┐
                  │ Local Documents  │
                  └────────┬─────────┘
                           │
                           ▼
                  ┌──────────────────┐
                  │ Document Loader  │
                  └────────┬─────────┘
                           │
                           ▼
                  ┌──────────────────┐
                  │    Tokenizer     │
                  └────────┬─────────┘
                           │
                           ▼
                  ┌──────────────────┐
                  │    Normalizer    │
                  └────────┬─────────┘
                           │
                           ▼
             ┌────────────────────────────┐
             │      Inverted Index        │
             │         Hash Table         │
             └─────────────┬──────────────┘
                           │
                    ┌──────┴──────┐
                    │ Search Query│
                    └──────┬──────┘
                           ▼
                    Posting Lists
                           │
                           ▼
                    Query Processing
                     AND / OR / NOT
                           │
                           ▼
                       Ranking
                           │
                           ▼
                      Top-K Heap
                           │
                           ▼
                    Search Results
```

---

# 7. Indexing

## 7.1 Document ID

모든 문서에는 내부적으로 고유한 정수 ID를 부여한다.

```cpp
using DocumentId = uint32_t;
```

예:

```text
0 → data_structure.md
1 → android.md
2 → operating_system.md
```

별도의 Document Table에서 ID와 문서 Metadata를 관리한다.

```cpp
struct Document {
    DocumentId id;
    std::string path;
    std::string title;
    uint32_t tokenCount;
};
```

---

## 7.2 Tokenization

문서의 원문을 검색 가능한 Token으로 분할한다.

예:

```text
"자료구조를 공부하고 있습니다."
```

초기 구현:

```text
자료구조를
공부하고
있습니다
```

한국어 처리 기능 적용 시:

```text
자료구조
공부
```

---

# 8. Inverted Index

검색 엔진의 핵심 자료구조이다.

일반적인 문서 저장 구조가

```text
Document → Words
```

라면 Inverted Index에서는 관계를 반대로 저장한다.

```text
Word → Documents
```

예:

```text
자료구조
→ doc1
→ doc7
→ doc21

알고리즘
→ doc2
→ doc7

안드로이드
→ doc4
→ doc12
```

C++ 구조 예시:

```cpp
struct Posting {
    DocumentId documentId;
    uint32_t termFrequency;
};

std::unordered_map<
    std::string,
    std::vector<Posting>
> invertedIndex;
```

각 Token이 Hash Table의 Key가 되고 해당 Token을 포함하는 문서들의 목록이 Posting List가 된다.

---

# 9. Posting List

Posting은 특정 단어와 문서 사이의 관계를 표현한다.

예:

```text
"자료구조"

[
    {doc1, frequency=12},
    {doc7, frequency=5},
    {doc21, frequency=2}
]
```

P0에서는 다음 정보만 저장한다.

```text
Document ID
Term Frequency
```

필요한 경우 추후 다음 정보까지 확장할 수 있다.

```text
Token Position
Field
문서 길이
```

Token Position을 저장하면 구문 검색을 구현할 수 있다.

예:

```text
"자료 구조"
```

처럼 인접한 단어를 검색하는 Phrase Search 구현이 가능하다.

---

# 10. Query Processing

## 단일 키워드

```text
Query:
자료구조
```

Hash Table에서 바로 검색한다.

```cpp
index.find("자료구조");
```

검색 대상 문서 전체를 읽을 필요 없이 해당 Token의 Posting List만 가져온다.

---

## AND Search

```text
자료구조 AND 알고리즘
```

각 Posting List의 교집합을 계산한다.

```text
자료구조
{1, 3, 7, 12}

알고리즘
{2, 3, 7, 15}

intersection

{3, 7}
```

---

## OR Search

두 Posting List의 합집합을 계산한다.

```text
{1, 2, 3, 7, 12, 15}
```

---

# 11. Ranking

검색 조건을 만족하는 모든 문서를 동일하게 표시하지 않고 관련도를 계산하여 순위를 부여한다.

## P0 Ranking

단순 Term Frequency를 사용한다.

```text
검색어: android

doc1 → 12회
doc2 → 5회
doc3 → 2회
```

따라서:

```text
doc1 > doc2 > doc3
```

---

## P1 Ranking

TF-IDF 또는 BM25 방식으로 확장한다.

이를 통해 모든 문서에서 자주 등장하는 일반적인 단어의 영향력을 낮추고 특정 문서를 잘 구분하는 단어의 중요도를 높인다.

---

# 12. Heap / Priority Queue

Ranking 결과 전체를 완전히 정렬할 필요 없이 가장 높은 K개의 검색 결과만 필요하다.

예:

```text
후보 문서: 10,000개
사용자가 보는 검색 결과: 10개
```

모든 결과 정렬:

```text
O(N log N)
```

크기 K의 Min Heap을 사용하는 Top-K:

```text
O(N log K)
```

구조:

```text
Candidates
    ↓
Score Calculation
    ↓
Min Heap (size = K)
    ↓
Top-K Results
```

Heap은 검색 Index를 저장하는 구조가 아니라 **검색 과정에서 상위 결과를 선택하기 위한 임시 자료구조**로 사용한다.

---

# 13. Trie 자동완성

P1 또는 P2에서 검색어 자동완성을 제공한다.

예:

```text
사용자 입력:

"안드"
```

결과:

```text
안드로이드
안드로이드스튜디오
안드로이드앱
```

Indexing 과정에서 발견된 Vocabulary를 Trie에 저장한다.

```text
          root
           │
           안
           │
           드
           │
           로
           │
           이
           │
           드
```

검색어 Prefix를 기준으로 관련 Token을 탐색한다.

---

# 14. 한국어 검색

한국어에서는 조사와 어미로 인해 동일한 단어가 다른 Token으로 인식될 수 있다.

예:

```text
자료구조를
자료구조가
자료구조는
자료구조의
```

단순 공백 Tokenizer에서는 서로 다른 네 개의 Token이 된다.

따라서 검색 성능 개선을 위해 Tokenizer 계층을 별도로 구성한다.

```cpp
class Tokenizer {
public:
    virtual std::vector<std::string>
    tokenize(const std::string& text) = 0;
};
```

### SimpleTokenizer

P0에서 사용한다.

* 공백 분리
* 특수문자 제거
* 영문 lowercase 변환

### KoreanTokenizer

P1 이후 선택적으로 적용한다.

후보:

* Kiwi C++ 형태소 분석기

형태소 분석을 통해 조사/어미 등의 기능어를 제거하고 의미 있는 형태소를 Index에 저장한다.

```text
자료구조를 공부한다

↓ Kiwi

자료구조 / NNG
를       / JKO
공부     / NNG
하       / VV
ㄴ다     / EF

↓

자료구조
공부
```

한국어 NLP 구현 자체가 프로젝트의 주요 목표가 되지 않도록 외부 라이브러리를 활용한다.

---

# 15. 자료구조별 역할

| 자료구조           | 역할                                |
| -------------- | --------------------------------- |
| Hash Table     | Token → Posting List 탐색           |
| Vector         | Posting List 저장                   |
| Set / 정렬 리스트   | AND / OR 검색                       |
| Heap           | 검색 결과 Top-K                       |
| Trie           | 검색어 자동완성                          |
| Array / Vector | Document Metadata 관리              |
| Graph          | 향후 ANN Semantic Search 구현 시 활용 가능 |

---

# 16. 개발 단계

## P0 — Core Search Engine

**목표: 검색 엔진의 핵심 자료구조 구현**

필수 기능:

* TXT / Markdown 문서 읽기
* Tokenization
* Inverted Index 생성
* Hash Table 기반 Token lookup
* Posting List
* 단일 Keyword Search
* 단순 Ranking
* Top-K 결과
* 문서 추가 및 전체 Re-indexing

완료 조건:

```text
문서 디렉토리 입력
       ↓
Index 생성
       ↓
검색어 입력
       ↓
관련 문서 Top-K 반환
```

---

## P1 — Advanced Lexical Search

**목표: 검색 품질 및 자료구조 활용 확대**

기능:

* AND / OR 검색
* TF-IDF 또는 BM25 Ranking
* Trie 기반 자동완성
* 한국어 Tokenization 개선
* Kiwi 연동 검토
* Stop Word 처리
* Index 저장 / 로드

선택 기능:

* Phrase Search
* Token Position Index

---

## P2 — Semantic Search Experimental

**목표: Keyword Search의 한계 비교**

EmbeddingGemma 등의 Embedding Model을 이용하여 각 문서를 Vector로 표현한다.

```text
Document
   ↓
EmbeddingGemma
   ↓
Embedding Vector
```

사용자 Query 역시 Embedding으로 변환한다.

```text
Query
   ↓
EmbeddingGemma
   ↓
Query Vector
```

두 Vector의 Cosine Similarity를 이용하여 의미적으로 유사한 문서를 검색한다.

P2에서는 단순 Linear Search도 허용한다.

이 단계의 목적은 Semantic Search 자체를 프로젝트의 핵심으로 삼는 것이 아니라 기존 Keyword Search와 결과 특성을 비교하는 것이다.

---

## P3 — Hybrid / ANN Search

시간 및 프로젝트 범위가 허용될 경우 구현한다.

후보 기능:

* Keyword + Semantic Hybrid Search
* ANN Index
* HNSW 등 Graph 기반 Vector Search
* Keyword 결과와 Semantic 결과 결합

예:

```text
Final Score
 =
α × BM25
+
β × Semantic Similarity
```

P3는 선택적인 연구/확장 단계이며 프로젝트 핵심 평가 범위에는 포함하지 않는다.

---

# 17. 검색 방식 비교

최종 프로젝트에서는 최소 두 가지 검색 방식을 비교한다.

## Naive Search

매 검색마다 모든 문서 검색:

```text
Query
  ↓
All Documents Scan
```

## Indexed Search

```text
Query
  ↓
Hash Lookup
  ↓
Posting List
```

비교 항목:

* 검색 시간
* 문서 수 증가에 따른 검색 시간 변화
* Indexing 시간
* Index 메모리 사용량

테스트 예:

```text
100 documents
1,000 documents
10,000 documents
```

이를 통해 자료구조 사용에 따른 성능 개선을 실험적으로 확인한다.

---

# 18. 성공 기준

P0 기준 프로젝트 성공 조건:

1. 최소 수백~수천 개 이상의 문서를 인덱싱할 수 있다.
2. 검색 시 모든 원본 문서를 다시 읽지 않는다.
3. Inverted Index를 이용해 키워드가 포함된 문서를 탐색한다.
4. 관련도에 따라 Top-K 결과를 제공한다.
5. 동일 데이터에 대해 Naive Search보다 검색 시간이 감소한다.
6. 사용된 각 자료구조의 역할과 선택 이유를 설명할 수 있다.

추가 평가:

* Indexing 시간
* Query Latency
* Memory Usage
* 검색 결과 Precision
* 문서 수 증가에 따른 성능 변화

---

# 19. 비목표 / Out of Scope

기본 프로젝트에서는 다음 기능을 목표로 하지 않는다.

* 웹 전체 Crawling
* Google 수준의 검색 품질
* 분산 검색 시스템
* 대규모 Vector Database
* LLM 기반 답변 생성
* 이미지 / 영상 검색
* 실시간 인터넷 검색
* 자체 형태소 분석기 개발

본 프로젝트의 핵심은 검색 서비스를 만드는 것이 아니라 **검색 문제를 적절한 자료구조로 해결하는 과정**에 있다.

---

# 20. 예상 기술 스택

### Core

* C++17 이상
* CMake
* STL

### 주요 STL 후보

```cpp
std::unordered_map
std::vector
std::set
std::unordered_set
std::priority_queue
```

Trie 등 핵심 자료구조는 직접 구현할 수 있다.

### Optional

* Kiwi — 한국어 형태소 분석
* EmbeddingGemma — Semantic Search
* GoogleTest / Catch2 — Unit Test

---

# 21. 프로젝트 핵심 설명

본 프로젝트의 핵심 아이디어는 다음 한 문장으로 정리할 수 있다.

> 모든 문서를 검색 시점에 순차 탐색하는 대신, 문서의 Token과 문서 간 관계를 Inverted Index로 사전에 구조화하여 빠르게 검색하고, Heap 등의 자료구조를 이용해 관련성이 높은 검색 결과를 효율적으로 반환한다.

프로젝트가 확장되더라도 중심 구조는 다음과 같이 유지한다.

```text
                  Search Engine
                       │
          ┌────────────┴────────────┐
          │                         │
   Lexical Search             Semantic Search
   [Main / P0-P1]             [Optional / P2-P3]
          │                         │
   Inverted Index               Embedding
   Hash / Set / Heap                │
          │                    Vector Search
          └────────────┬────────────┘
                       │
                 Hybrid Ranking
```

따라서 Semantic Search를 추가하더라도 프로젝트의 핵심은 자료구조 기반 Lexical Search Engine으로 유지한다.

