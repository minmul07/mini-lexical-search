#pragma once

namespace mini_lexical_search {

inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 1;
inline constexpr int kVersionPatch = 0;

// semantic version 문자열을 반환한다. 예: "0.1.0".
const char* versionString();

}  // namespace mini_lexical_search
