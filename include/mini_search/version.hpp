#pragma once

namespace mini_search {

inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 1;
inline constexpr int kVersionPatch = 0;

// Returns the semantic version string, e.g. "0.1.0".
const char* versionString();

}  // namespace mini_search
