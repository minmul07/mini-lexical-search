# FTXUI 사용 승인 및 적용 범위

## 라이브러리와 허용 용도

[FTXUI (Functional Terminal User Interface)](https://github.com/ArthurSonzogni/FTXUI)는 C++ 기반 터미널 사용자 인터페이스 라이브러리이다. 사용 범위는 다음 UI 기능으로 한정한다.

* 검색어 입력
* 검색 결과 목록 및 문서 정보 표시·레이아웃
* 키보드 입력 및 화면 전환
* 인덱싱 진행 상태 등 프로그램 상태의 시각적 표시

문서 분석·토큰화, 역색인 구축·저장, 검색 처리, 점수 계산, 결과 정렬, Heap Top-K 및 Trie 등 핵심 자료구조·알고리즘은 STL과 직접 작성한 C++ 코드로 구현한다. FTXUI는 이 기능을 구현하는 데 사용하지 않는다. 핵심 로직과 UI를 분리하고, UI는 핵심 로직의 결과를 받아 표시한다.

## 제출 및 빌드 조건

* FTXUI 소스를 프로젝트 내부에 포함(vendoring)하여 함께 제출한다. amalgamated 배포본 7.0.3(`f921fad`)을 `third_party/ftxui/`에 두었다. 빌드는 split 파일(`ftxui.hpp`, `ftxui.cpp`)과 `LICENSE`를 사용하며, CMake 타깃 `ftxui`가 pthread와 함께 컴파일한다. 핵심 모듈과 CLI는 이 타깃에 링크하지 않는다.
* 라이브러리와 직접 작성한 코드를 포함한 최종 제출 용량은 10MB를 초과하지 않아야 한다.
* Ubuntu 24.04 LTS 64-bit, GNU g++ 및 ISO C++20(`-std=c++20`) 환경에서 별도 패키지 설치나 네트워크 다운로드 없이 `make` 한 번으로 실행 파일을 생성해야 한다.
* 실행 시 `--no-tui` 옵션으로 FTXUI 화면 없이 CLI에서 핵심 검색 기능을 독립적으로 사용할 수 있어야 한다.

## 현재 상태와 후속 검증

핵심 로직과 CLI는 `SearchApplication`을 통해 분리되었으며 인덱싱·검색 결과와 진행 이벤트를 UI 없이 제공한다. `start`의 기본 경로는 FTXUI TUI이며 `--no-tui`로 CLI REPL을 실행한다. 현재 TUI는 터미널 중앙에 `on development`만 표시하는 플레이스홀더이고, 검색 UI는 아직 연결하지 않는다. FTXUI는 `mini_lexical_search_tui`에만 링크한다. 제출용 Makefile은 아직 구현되지 않았다. 현재 빌드·실행 방법은 [개발 환경](DEVELOPMENT.md)을 따른다.

검색 TUI 도입 후에는 [테스트 가이드](TESTING.md)의 핵심 기능·CLI 검증에 더해 다음을 확인한다.

* 깨끗한 채점 환경에서 네트워크·별도 설치 없이 `make` 및 프로그램 실행 성공
* 동일 문서·질의에 대한 TUI와 `--no-tui` CLI의 검색 결과 일치
* 검색어 입력, 결과 표시, 키보드 조작, 화면 전환 및 상태 표시
* 핵심 모듈의 FTXUI 의존성 부재와 최종 제출 용량 10MB 이하
