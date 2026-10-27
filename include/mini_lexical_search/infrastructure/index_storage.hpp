#pragma once

#include <filesystem>
#include "mini_lexical_search/domain/index_snapshot.hpp"

namespace mini_lexical_search {

// snapshot 파일을 읽는다. 파일이 없으면 빈 snapshot을 반환한다.
// 손상된 데이터는 덮어쓰지 않고 거부한다.
IndexSnapshot loadIndexSnapshot(const std::filesystem::path& input);

// snapshot을 원자적으로 저장하며, 저장 실패 시 기존 파일을 보존한다.
void saveIndexSnapshot(const std::vector<Document>& documents,
                       const InvertedIndex& index,
                       const std::filesystem::path& output);

}  // namespace mini_lexical_search
