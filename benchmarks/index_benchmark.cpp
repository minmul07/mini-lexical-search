#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include "mini_lexical_search/application/indexer.hpp"
#include "mini_lexical_search/infrastructure/index_storage.hpp"

using Clock = std::chrono::steady_clock;
using namespace mini_lexical_search;

template<class Operation> double measure(Operation operation) {
    const auto start = Clock::now();
    operation();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// 첫 실행은 워밍업이며 출력하지 않는다. 매 반복은 빈 스냅샷에서 시작한다.
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("usage: index_benchmark <documents> <new-output.bin>");
        const std::filesystem::path output = argv[2];
        if (std::filesystem::exists(output)) throw std::runtime_error("output must not exist");
        std::cout << "run,index_ms,save_ms,load_ms,bytes,documents,terms,postings,lines\n";
        for (int run = 0; run <= 7; ++run) {
            IndexSnapshot snapshot;
            SimpleTokenizer tokenizer;
            const double indexMs = measure([&] { updateIndexSnapshot(argv[1], snapshot, output, tokenizer); });
            const double saveMs = measure([&] { saveIndexSnapshot(snapshot.documents, snapshot.index, output); });
            IndexSnapshot loaded;
            const double loadMs = measure([&] { loaded = loadIndexSnapshot(output); });
            if (snapshot.documents.size() != loaded.documents.size() ||
                snapshot.index.termCount() != loaded.index.termCount())
                throw std::runtime_error("round trip counts differ");
            std::size_t lines = 0;
            for (const auto& [term, postings] : snapshot.index.postings()) {
                const auto* actual = loaded.index.find(term);
                if (!actual || actual->size() != postings.size()) throw std::runtime_error("postings differ");
                for (std::size_t i = 0; i < postings.size(); ++i) {
                    if (postings[i].documentId != (*actual)[i].documentId ||
                        postings[i].termFrequency != (*actual)[i].termFrequency ||
                        postings[i].lines != (*actual)[i].lines) throw std::runtime_error("posting content differs");
                    lines += postings[i].lines.size();
                }
            }
            if (run) std::cout << run << ',' << std::fixed << std::setprecision(3)
                << indexMs << ',' << saveMs << ',' << loadMs << ',' << std::filesystem::file_size(output)
                << ',' << snapshot.documents.size() << ',' << snapshot.index.termCount()
                << ',' << snapshot.index.postingCount() << ',' << lines << '\n';
            if (run != 7) std::filesystem::remove(output);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
