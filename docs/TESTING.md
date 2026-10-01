# 테스트 가이드

CLI 또는 명령 처리 변경 시 다음 검증을 수행한다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

* `version_test`: 버전 문자열 확인
* `cli_test`: `start` 실행, 실행 시 도움말·버전 옵션, 잘못된 실행 인자, 실행 중 슬래시 명령, 잘못된 명령 이후 입력 처리, 빈 입력, EOF 및 종료 명령 확인

대화형 입력과 프롬프트는 `./build/mini_lexical_search start`로 직접 실행하여 `/index`, `/search 자료구조`, `/help`, `/exit`를 입력해 확인한다. 현재 인덱싱·검색은 미구현 안내만 출력하므로, 이 테스트는 실제 색인이나 검색 결과를 검증하지 않는다.
