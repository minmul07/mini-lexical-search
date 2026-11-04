#include "mini_lexical_search/infrastructure/index_storage.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "mini_lexical_search/application/indexer.hpp"

namespace fs = std::filesystem;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "could not read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

template <typename Operation>
void requireFailure(Operation operation, const std::string& message) {
    bool failed = false;
    try {
        operation();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed, message);
}

std::uint8_t hexDigit(char character) {
    if (character >= '0' && character <= '9') {
        return static_cast<std::uint8_t>(character - '0');
    }
    return static_cast<std::uint8_t>(character - 'a' + 10);
}

// index_binary.cpp의 v1 형식을 테스트에서 독립적으로 구성한다.
struct SnapshotBytes {
    std::string bytes;

    void u8(std::uint8_t value) { bytes.push_back(static_cast<char>(value)); }

    void u32(std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            bytes.push_back(static_cast<char>((value >> shift) & 0xffu));
        }
    }

    void string(const std::string& value) {
        u32(static_cast<std::uint32_t>(value.size()));
        bytes += value;
    }

    void document(std::uint32_t id,
                  const std::string& path,
                  const std::string& title,
                  std::uint32_t tokenCount,
                  const std::string& hash = {}) {
        u32(id);
        string(path);
        string(title);
        u32(tokenCount);
        if (hash.empty()) {
            u8(0);
            return;
        }
        u8(32);
        for (std::size_t i = 0; i < hash.size(); i += 2) {
            u8(static_cast<std::uint8_t>((hexDigit(hash[i]) << 4) | hexDigit(hash[i + 1])));
        }
    }

    void posting(std::uint32_t documentId,
                 std::uint32_t termFrequency,
                 const std::vector<std::uint32_t>& lines) {
        u32(documentId);
        u32(termFrequency);
        u32(static_cast<std::uint32_t>(lines.size()));
        for (const auto line : lines) u32(line);
    }
};

SnapshotBytes snapshotHeader(std::uint32_t documentCount) {
    SnapshotBytes snapshot;
    snapshot.bytes = "MLSI";
    snapshot.u32(1);
    snapshot.u32(documentCount);
    return snapshot;
}

int main(int argc, char* argv[]) {
    try {
        require(argc == 2, "expected a test fixture directory");
        const fs::path root = fs::absolute(argv[1]);
        fs::remove_all(root);
        const auto docs = root / "docs";
        const auto snapshotPath = root / "index.bin";
        fs::create_directories(docs);
        std::ofstream(docs / "a.txt") << "자료구조 자료구조\n자료구조\n";
        std::ofstream(docs / "b.md") << "자료구조 알고리즘\n";

        mini_lexical_search::SimpleTokenizer tokenizer;
        mini_lexical_search::IndexSnapshot snapshot;

        // A missing snapshot is not an error; it means nothing was indexed yet.
        const auto missing = mini_lexical_search::loadIndexSnapshot(snapshotPath);
        require(missing.documents.empty() && missing.index.empty(),
                "missing snapshot must load empty");

        // First build: every document is added and tokenized.
        auto update =
            mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.added == 2 && update.metadata.notUpdated == 0 &&
                    update.metadata.updated == 0 && update.metadata.deleted == 0,
                "first build metadata failed");
        require(update.tokens.documents == 2 && update.tokens.tokens == 5,
                "first build tokenization failed");
        require(snapshot.documents.size() == 2 && snapshot.documents[0].id == 0 &&
                    snapshot.documents[1].id == 1,
                "first build document table failed");
        require(snapshot.documents[0].tokenCount == 3 && snapshot.documents[1].tokenCount == 2,
                "tokenCount must be stored on the documents");
        const auto* structure = snapshot.index.find("자료구조");
        require(structure != nullptr && structure->size() == 2,
                "자료구조 must be indexed for both documents");
        require((*structure)[0].documentId == 0 && (*structure)[0].termFrequency == 3 &&
                    (*structure)[0].lines == std::vector<std::uint32_t>{1, 2},
                "first document posting failed");
        require((*structure)[1].documentId == 1 && (*structure)[1].termFrequency == 1 &&
                    (*structure)[1].lines == std::vector<std::uint32_t>{1},
                "second document posting failed");
        require(snapshot.index.find("알고리즘") != nullptr, "알고리즘 must be indexed");

        // The stored file reloads into an equivalent in-memory snapshot.
        const auto reloaded = mini_lexical_search::loadIndexSnapshot(snapshotPath);
        require(reloaded.documents.size() == 2 &&
                    reloaded.documents[1].path == snapshot.documents[1].path &&
                    reloaded.documents[1].hash == snapshot.documents[1].hash,
                "reload must preserve the document table");
        const auto* reloadedStructure = reloaded.index.find("자료구조");
        require(reloadedStructure != nullptr && reloadedStructure->size() == 2 &&
                    (*reloadedStructure)[0].termFrequency == 3 &&
                    (*reloadedStructure)[1].lines == std::vector<std::uint32_t>{1},
                "reload must preserve postings");

        // The serialized binary format is deterministic.
        const auto tinyPath = root / "tiny.bin";
        mini_lexical_search::IndexSnapshot tiny;
        tiny.documents.push_back({0, "a.txt", "a.txt", 1, ""});
        tiny.index.add("a", 0, 1);
        mini_lexical_search::saveIndexSnapshot(tiny.documents, tiny.index, tinyPath);
        auto expected = snapshotHeader(1);
        expected.document(0, "a.txt", "a.txt", 1);
        expected.u32(1);
        expected.string("a");
        expected.u32(1);
        expected.posting(0, 1, {1});
        require(read(tinyPath) == expected.bytes, "snapshot binary format failed");

        mini_lexical_search::saveIndexSnapshot({}, {}, tinyPath);
        auto emptyExpected = snapshotHeader(0);
        emptyExpected.u32(0);
        require(read(tinyPath) == emptyExpected.bytes, "empty snapshot format failed");

        // An unchanged directory skips tokenization and keeps the file intact.
        const auto before = read(snapshotPath);
        const auto timestamp = fs::file_time_type::clock::now() - std::chrono::hours(24);
        fs::last_write_time(snapshotPath, timestamp);
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.notUpdated == 2 && update.metadata.added == 0 &&
                    update.metadata.updated == 0 && update.metadata.deleted == 0,
                "unchanged metadata failed");
        require(update.tokens.documents == 0 && update.tokens.tokens == 0,
                "unchanged documents must not be tokenized");
        require(read(snapshotPath) == before && fs::last_write_time(snapshotPath) == timestamp,
                "unchanged run must not rewrite the snapshot");

        // An edited document is rebuilt from scratch; other postings stay.
        std::ofstream(docs / "a.txt") << "자료구조 변경\n";
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.updated == 1 && update.metadata.notUpdated == 1 &&
                    update.tokens.documents == 1 && update.tokens.tokens == 2,
                "updated metadata/tokenization failed");
        structure = snapshot.index.find("자료구조");
        require(structure != nullptr && structure->size() == 2 &&
                    (*structure)[0].documentId == 0 && (*structure)[0].termFrequency == 1 &&
                    (*structure)[0].lines == std::vector<std::uint32_t>{1},
                "old postings of the edited document must be discarded");
        const auto* changed = snapshot.index.find("변경");
        require(changed != nullptr && changed->size() == 1 && (*changed)[0].documentId == 0,
                "new tokens of the edited document must be indexed");
        require(snapshot.index.find("알고리즘") != nullptr,
                "postings of unchanged documents must survive");
        require(snapshot.documents[0].tokenCount == 2, "edited tokenCount must be rebuilt");

        // A deleted document drops its postings without tokenization.
        fs::remove(docs / "b.md");
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.deleted == 1 && update.metadata.updated == 0 &&
                    update.metadata.added == 0,
                "deletion metadata failed");
        require(update.tokens.documents == 0 && update.tokens.tokens == 0,
                "deletion must not tokenize");
        require(snapshot.index.find("알고리즘") == nullptr,
                "deleted document postings must be removed");
        require(snapshot.index.find("자료구조")->size() == 1,
                "other postings must survive a deletion");

        // Tokenless documents are stored with an empty posting contribution.
        std::ofstream(docs / "blank.txt") << "!!!\n";
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.added == 1 && update.tokens.documents == 1 &&
                    update.tokens.tokens == 0,
                "tokenless document failed");
        require(snapshot.documents.size() == 2 && snapshot.documents[1].tokenCount == 0,
                "tokenless tokenCount must be zero");

        // A failed save must keep both the stored file and the in-memory state.
        const auto goodSnapshot = read(snapshotPath);
        std::ofstream(docs / "a.txt") << "다시\n";
        fs::create_directories(snapshotPath.string() + ".tmp");
        std::ofstream(fs::path(snapshotPath.string() + ".tmp") / "blocker") << "blocked";
        requireFailure(
            [&] {
                mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
            },
            "blocked save must fail");
        require(read(snapshotPath) == goodSnapshot,
                "failed save must not change the stored snapshot");
        require(snapshot.index.find("다시") == nullptr,
                "failed save must roll back the in-memory postings");
        require(snapshot.index.find("변경") != nullptr,
                "rollback must restore the previous postings");
        fs::remove_all(snapshotPath.string() + ".tmp");
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.updated == 1 && update.tokens.tokens == 1 &&
                    snapshot.index.find("다시") != nullptr &&
                    snapshot.index.find("변경") == nullptr,
                "retry after a failed save must rebuild the postings");

        // Damaged snapshots are rejected instead of being overwritten.
        const auto checkCorrupt = [&](const std::string& name, const std::string& bytes) {
            const auto path = root / (name + ".bin");
            std::ofstream(path, std::ios::binary) << bytes;
            requireFailure([&] { mini_lexical_search::loadIndexSnapshot(path); },
                           "damaged snapshot accepted: " + name);
        };

        checkCorrupt("empty", "");
        checkCorrupt("truncated_header", "MLSI");

        auto badMagic = snapshotHeader(0);
        badMagic.bytes[0] = 'N';
        badMagic.u32(0);
        checkCorrupt("bad_magic", badMagic.bytes);

        auto badVersion = snapshotHeader(0);
        badVersion.bytes[4] = 2;
        badVersion.u32(0);
        checkCorrupt("bad_version", badVersion.bytes);

        checkCorrupt("trailing", expected.bytes + "x");
        checkCorrupt("truncated_document", expected.bytes.substr(0, expected.bytes.size() - 1));

        auto hugeDocuments = snapshotHeader(0xffffffffu);
        checkCorrupt("huge_document_count", hugeDocuments.bytes);

        auto duplicateId = snapshotHeader(2);
        duplicateId.document(0, "a", "a", 0);
        duplicateId.document(0, "b", "b", 0);
        duplicateId.u32(0);
        checkCorrupt("duplicate_document_id", duplicateId.bytes);

        auto duplicatePath = snapshotHeader(2);
        duplicatePath.document(0, "a", "a", 0);
        duplicatePath.document(1, "a", "b", 0);
        duplicatePath.u32(0);
        checkCorrupt("duplicate_document_path", duplicatePath.bytes);

        auto unknownPostingDocument = snapshotHeader(1);
        unknownPostingDocument.document(0, "a", "a", 0);
        unknownPostingDocument.u32(1);
        unknownPostingDocument.string("a");
        unknownPostingDocument.u32(1);
        unknownPostingDocument.posting(1, 1, {1});
        checkCorrupt("unknown_posting_document", unknownPostingDocument.bytes);

        auto emptyTerm = snapshotHeader(0);
        emptyTerm.u32(1);
        emptyTerm.string("");
        emptyTerm.u32(1);
        emptyTerm.posting(0, 1, {1});
        checkCorrupt("empty_term", emptyTerm.bytes);

        auto badHash = snapshotHeader(1);
        badHash.u32(0);
        badHash.string("a");
        badHash.string("a");
        badHash.u32(0);
        badHash.u8(31);
        badHash.u32(0);
        checkCorrupt("bad_hash_length", badHash.bytes);

        auto unsortedPostings = snapshotHeader(2);
        unsortedPostings.document(0, "a", "a", 0);
        unsortedPostings.document(1, "b", "b", 0);
        unsortedPostings.u32(1);
        unsortedPostings.string("a");
        unsortedPostings.u32(2);
        unsortedPostings.posting(1, 1, {1});
        unsortedPostings.posting(0, 1, {1});
        checkCorrupt("unsorted_postings", unsortedPostings.bytes);

        auto zeroTermFrequency = snapshotHeader(1);
        zeroTermFrequency.document(0, "a", "a", 0);
        zeroTermFrequency.u32(1);
        zeroTermFrequency.string("a");
        zeroTermFrequency.u32(1);
        zeroTermFrequency.posting(0, 0, {1});
        checkCorrupt("zero_term_frequency", zeroTermFrequency.bytes);

        auto zeroLine = snapshotHeader(1);
        zeroLine.document(0, "a", "a", 0);
        zeroLine.u32(1);
        zeroLine.string("a");
        zeroLine.u32(1);
        zeroLine.posting(0, 1, {0});
        checkCorrupt("zero_line", zeroLine.bytes);

        auto frequencyBelowLines = snapshotHeader(1);
        frequencyBelowLines.document(0, "a", "a", 0);
        frequencyBelowLines.u32(1);
        frequencyBelowLines.string("a");
        frequencyBelowLines.u32(1);
        frequencyBelowLines.posting(0, 1, {1, 2});
        checkCorrupt("frequency_below_lines", frequencyBelowLines.bytes);

        auto duplicateLine = snapshotHeader(1);
        duplicateLine.document(0, "a", "a", 0);
        duplicateLine.u32(1);
        duplicateLine.string("a");
        duplicateLine.u32(1);
        duplicateLine.posting(0, 2, {1, 1});
        checkCorrupt("duplicate_line", duplicateLine.bytes);

        auto hugeTerms = snapshotHeader(0);
        hugeTerms.u32(0xffffffffu);
        checkCorrupt("huge_term_count", hugeTerms.bytes);

        auto hugePostings = snapshotHeader(0);
        hugePostings.u32(1);
        hugePostings.string("a");
        hugePostings.u32(0xffffffffu);
        checkCorrupt("huge_posting_count", hugePostings.bytes);

        auto hugeLines = snapshotHeader(0);
        hugeLines.u32(1);
        hugeLines.string("a");
        hugeLines.u32(1);
        hugeLines.u32(0);
        hugeLines.u32(1);
        hugeLines.u32(0xffffffffu);
        checkCorrupt("huge_line_count", hugeLines.bytes);

        std::cout << "indexer_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
