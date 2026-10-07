# 테스트 가이드

CLI 또는 명령 처리 변경 시 다음 검증을 수행한다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

* `version_test`: 버전 문자열 확인
* `tokenizer_test`: 공백 분리, 특수문자 제거, ASCII 소문자 변환, UTF-8 한국어 보존, 빈 줄·문장부호 전용 줄, 탭·CR 분리, 반복 토큰 유지 확인
* `document_test`: 재귀 수집, 확장자·심볼릭 링크 필터링, ID·경로·제목·`tokenCount=0`(수집 단계), JSON 파싱·이스케이프·UTF-8, 빈 목록 저장, 재저장, SHA-256 표준 벡터·다중 청크·바이너리, 네 가지 변경 집계, 기존 ID 보존, 변경 없는 JSON 내용·수정 시간 보존, 기존 스키마 전환, 잘못된 JSON 거부, 잘못된 입력 및 저장 실패 시 기존 파일 보존 확인. 추가로 변경 문서만 토큰화되는지, 토큰의 원본 1-based 줄 번호, 줄을 넘는 토큰 수 집계, `tokenCount` 기록, 게이트된 문서의 파일 미열람·JSON 미재저장, 빈 토큰 문서 처리 확인
* `inverted_index_test`: term 추가, `termFrequency` 누적, 같은 줄 중복 제거, 줄·문서 정렬 유지, 없는 term 조회, 문서 제거와 빈 term 정리, `merge`와 중복 문서 거부, `fromPostings` 형식 불변식(정렬·줄 번호·TF) 검증
* `indexer_test`: 스냅샷 왕복 저장·로드, 결정적 JSON 형식, 최초 전체 구축, 변경 없는 실행의 토큰화 생략·파일 미재저장, 변경 문서 Posting 재구축, 문서 삭제 시 Posting 제거, 빈 토큰 문서, 저장 실패 시 파일·메모리 보존과 재시도, 손상 스냅샷 거부 확인
* `cli_test`: `start` 실행, 실행 시 도움말·버전 옵션, 잘못된 실행 인자, 실행 중 슬래시 명령, `documents/recap` 입력 및 기본 `./documents` 입력의 실제 스냅샷 저장(`version`·`documents`·`invertedIndex`), 메타데이터·토큰화 단계 메시지와 ms 시간, 앱 재실행 후 스냅샷 로드·해시 비교·변경 집계 및 변경 없는 문서의 토큰화·재저장 생략, `tokenCount`와 Posting 저장, 삭제 문서 Posting 제거, 손상된 스냅샷 로드 실패와 덮어쓰기 거부, 잘못된 명령 이후 입력 처리, 빈 입력, EOF 및 종료 명령 확인

대화형 입력과 프롬프트는 `./build/mini_lexical_search start`로 직접 실행하여 `/index`, `/search 자료구조`, `/help`, `/exit`를 입력해 확인한다. `/index [디렉터리]`는 `.data/index.json`을 저장하므로 현재 작업 디렉터리의 `.data/`에 쓰기 권한이 필요하다. 자동 저장 테스트는 빌드 디렉터리의 격리된 경로를 사용한다. 검색은 미구현이므로 검색 결과는 검증하지 않는다.
