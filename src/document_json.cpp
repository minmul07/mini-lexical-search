#include "mini_lexical_search/document.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

#include "json_internal.hpp"

namespace mini_lexical_search {

std::vector<Document> loadDocuments(const std::filesystem::path& input) {
    std::ifstream stream(input, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot read document JSON: " + input.string());
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) throw std::runtime_error("cannot read document JSON: " + input.string());
    json::Reader reader(std::move(text), "document");
    auto documents = json::parseDocumentArray(reader);
    reader.finish();
    return documents;
}

}  // namespace mini_lexical_search
