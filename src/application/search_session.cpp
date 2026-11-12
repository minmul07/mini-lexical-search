#include "mini_lexical_search/application/search_application.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "mini_lexical_search/domain/query_processor.hpp"
#include "mini_lexical_search/application/log.hpp"
#include "mini_lexical_search/application/working_job.hpp"
#include "mini_lexical_search/infrastructure/index_storage.hpp"
#include "mini_lexical_search/infrastructure/document_io.hpp"
#include "mini_lexical_search/infrastructure/matching_lines.hpp"

namespace mini_lexical_search {
namespace {

// Merge sorted posting lines without copying their contents. Positions advance together
// for duplicate numbers, including a row that matches several distinct query terms.
struct LineCursor {
    std::vector<const std::vector<std::uint32_t>*> lists;
    std::vector<std::size_t> positions;

    std::uint32_t next() {
        std::uint32_t number = 0;
        for (std::size_t i = 0; i < lists.size(); ++i) {
            if (positions[i] < lists[i]->size()) {
                const auto candidate = (*lists[i])[positions[i]];
                if (number == 0 || candidate < number) number = candidate;
            }
        }
        if (number != 0) {
            for (std::size_t i = 0; i < lists.size(); ++i) {
                if (positions[i] < lists[i]->size() && (*lists[i])[positions[i]] == number) {
                    ++positions[i];
                }
            }
        }
        return number;
    }
};

bool byteLess(const std::string& left, const std::string& right) {
    return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end(),
        [](unsigned char a, unsigned char b) { return a < b; });
}

// Compare components with directories before files at each parent, rather than
// sorting the complete path string (where a root file could precede a directory).
bool treeLess(const Document* left, const Document* right) {
    const std::filesystem::path a(left->path), b(right->path);
    auto x = a.begin(), y = b.begin();
    while (x != a.end() && y != b.end()) {
        auto nx = std::next(x), ny = std::next(y);
        const bool dirX = nx != a.end(), dirY = ny != b.end();
        if (dirX != dirY) return dirX;
        if (*x != *y) return byteLess(x->generic_string(), y->generic_string());
        x = nx;
        y = ny;
    }
    return false;
}

struct FileState {
    SearchResult result;
    LineCursor cursor;
    bool counted = false;
    MatchingReadPosition position;

    void count() {
        if (counted) return;
        auto counter = cursor;
        while (counter.next() != 0) ++result.totalMatchingLines;
        counted = true;
    }

    // Only I/O failures are converted into per-file warnings. Sink exceptions propagate.
    void read(std::size_t limit, const std::function<void(const MatchingLine&)>& sink) {
        if (!result.warning.empty()) return;
        result.warning = readMatchingLineRange(result.document, position,
            [&] { return cursor.next(); }, limit, sink);
    }
};

}  // namespace

struct SearchApplication::Session {
    std::vector<FileState> files;
    std::vector<std::pair<std::size_t, std::size_t>> pages;
    std::size_t page = 0;
};

SearchApplication::SearchApplication(std::filesystem::path snapshotPath)
    : snapshotPath_(std::move(snapshotPath)) {
    workingJob().set("Loading index");
    const auto started = std::chrono::steady_clock::now();
    try {
        snapshot_ = loadIndexSnapshot(snapshotPath_);
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count();
        workingJob().set("Ready");
        appendLog("index load succeeded (" + formatElapsedMilliseconds(elapsed) + " ms)");
    } catch (const std::exception& error) {
        ready_ = false;
        loadError_ = error.what();
        workingJob().set("Failed to load index");
        appendLog("index load failed: " + loadError_);
    }
}

SearchApplication::~SearchApplication() = default;
void SearchApplication::clearSearch() { session_.reset(); }

void SearchApplication::startSearch(const std::string& text) {
    clearSearch();
    requireReady();
    const auto query = parseQuery(text, tokenizer_);
    const auto ids = QueryProcessor{}.matchingDocuments(query, snapshot_.index);
    std::unordered_map<DocumentId, const Document*> documents;
    for (const auto& document : snapshot_.documents) documents.emplace(document.id, &document);
    std::vector<const Document*> ordered;
    ordered.reserve(ids.size());
    for (const auto id : ids) ordered.push_back(documents.at(id));
    std::sort(ordered.begin(), ordered.end(), treeLess);
    auto session = std::make_unique<Session>();
    session->files.reserve(ordered.size());
    for (const auto* document : ordered) {
        FileState file;
        file.result.document = *document;
        file.result.relativePath = std::filesystem::path(document->path)
                                       .lexically_relative(snapshot_.root).generic_string();
        for (const auto& term : query.terms) {
            const auto* list = snapshot_.index.find(term);
            if (!list) continue;
            const auto posting = std::lower_bound(list->begin(), list->end(), document->id,
                [](const Posting& item, DocumentId id) { return item.documentId < id; });
            if (posting != list->end() && posting->documentId == document->id) {
                file.cursor.lists.push_back(&posting->lines);
                file.cursor.positions.push_back(0);
            }
        }
        session->files.push_back(std::move(file));
    }
    session_ = std::move(session);
}

void SearchApplication::addPage() {
    const auto first = session_->pages.empty() ? 0 : session_->pages.back().second;
    auto end = first;
    std::size_t lines = 0;
    while (end < session_->files.size() && lines < 50) {
        auto& file = session_->files[end++];
        file.count();
        file.read(10, [&](const MatchingLine& line) { file.result.lines.push_back(line); });
        lines += file.result.lines.size();
    }
    session_->pages.emplace_back(first, end);
}

SearchPage SearchApplication::search(const std::string& query) {
    startSearch(query);
    addPage();
    return currentPage();
}

void SearchApplication::requireSearch() const {
    if (!session_ || session_->pages.empty()) throw std::runtime_error("no active search; enter a query first");
}

SearchPage SearchApplication::currentPage() const {
    requireSearch();
    SearchPage result;
    const auto [first, end] = session_->pages[session_->page];
    result.number = session_->page + 1;
    result.hasPrevious = session_->page != 0;
    result.hasNext = end < session_->files.size();
    for (auto i = first; i < end; ++i) result.files.push_back(session_->files[i].result);
    return result;
}

SearchPage SearchApplication::nextPage() {
    requireSearch();
    if (session_->pages[session_->page].second < session_->files.size()) {
        if (session_->page + 1 == session_->pages.size()) addPage();
        ++session_->page;
    }
    return currentPage();
}

SearchPage SearchApplication::previousPage() {
    requireSearch();
    if (session_->page != 0) --session_->page;
    return currentPage();
}

SearchPage SearchApplication::expandFile(const std::string& relativePath) {
    requireSearch();
    const auto normalized = std::filesystem::path(relativePath).lexically_normal().generic_string();
    const auto [first, end] = session_->pages[session_->page];
    for (auto i = first; i < end; ++i) {
        auto& file = session_->files[i];
        if (file.result.relativePath == normalized) {
            if (file.result.lines.size() < file.result.totalMatchingLines) {
                file.read(10, [&](const MatchingLine& line) { file.result.lines.push_back(line); });
            }
            return currentPage();
        }
    }
    throw std::invalid_argument("file is not on the current page: " + relativePath);
}

std::vector<std::string> SearchApplication::readFile(const std::string& relativePath) const {
    requireSearch();
    const auto normalized = std::filesystem::path(relativePath).lexically_normal().generic_string();
    const auto [first, end] = session_->pages[session_->page];
    for (auto i = first; i < end; ++i) {
        const auto& file = session_->files[i].result;
        if (file.relativePath != normalized) continue;
        std::vector<std::string> lines;
        readDocumentLines(file.document, [&](std::uint32_t, const std::string& text) {
            lines.push_back(text);
        });
        return lines;
    }
    throw std::invalid_argument("file is not on the current page: " + relativePath);
}

void SearchApplication::visitAllResults(const std::string& query,
    const std::function<void(const SearchResult&)>& group,
    const std::function<void(const MatchingLine&)>& line,
    const std::function<void(const std::string&)>& warning) {
    startSearch(query);
    // A streaming query deliberately does not expose a pageable session.
    const auto session = std::move(session_);
    for (auto& file : session->files) {
        file.count();
        group(file.result);
        file.read(std::numeric_limits<std::size_t>::max(), line);
        if (!file.result.warning.empty()) warning(file.result.warning);
    }
}

}  // namespace mini_lexical_search
