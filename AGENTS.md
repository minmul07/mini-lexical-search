## Documentation

After implementing a change, inspect relevant project documentation.

Update documentation in the same change when the implementation changes:
- externally observable behavior
- architecture or module responsibilities
- APIs or data contracts
- setup, build, or development workflows
- non-obvious technical constraints

Do not update documentation for implementation-only refactors that do not
change documented behavior.

Keep documentation changes minimal and scoped to the affected area.
Do not rewrite unrelated documentation.

Do not modify AGENTS.md automatically.
If you discover a recurring project rule, constraint, or workflow that should
be preserved for future agents, suggest an AGENTS.md change to the user.

---

## 작업 완료 응답 형식

작업 완료 후 필요한 항목만 간결히 보고한다.

### 변경 사항

* 변경한 내용과 주요 파일

### 검증

* 실행한 검증과 결과

### 가정 / 남은 작업

* 중요한 가정, 미검증 사항, 남은 TODO가 있는 경우에만 작성

사용하지 않은 도구를 모두 나열하지 않는다.

수행하지 않은 검증은 결과의 신뢰성에 영향을 주는 경우에만 이유를 보고한다.

위험하거나 불확실한 부분은 조용히 추측하지 말고 명시적으로 보고한다.

---

## 완료 기준

작업은 다음 조건을 만족해야 완료된 것으로 본다.

1. 요청된 동작 또는 변경이 구현되어 있다.
2. 요청과 무관한 파일이나 동작을 변경하지 않았다.
3. `docs/TESTING.md` 기준에 맞는 필요한 검증을 수행했다.
4. 검증이 실패하거나 수행할 수 없는 경우 원인과 영향 범위를 명확히 보고했다.
5. 외부 API, 백엔드 또는 불명확한 동작에 대한 중요한 가정을 명시했다.
6. 위험 요소나 남은 작업이 있다면 숨기지 않고 보고했다.

---

## 개발 조건

1. C++로 구현할 것

2. 환경: OS: Ubuntu 24.04 LTS (64-bit) / Compiler: GNU g++ (GCC) / C++ Standard: ISO C++20 / Compile Option: -std=c++20

3.  FTXUI를 제외한 외부 라이브러리는 사용 불가합니다. `2. 환경`에서 명세한 채점환경에서 사용 가능한 내부 라이브러리만 사용 가능합니다.