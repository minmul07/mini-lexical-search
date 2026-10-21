#include "mini_lexical_search/application/working_job.hpp"

#include <utility>

namespace mini_lexical_search {

void WorkingJob::set(std::string label) {
    label_ = std::move(label);
}

const std::string& WorkingJob::label() const {
    return label_;
}

WorkingJob& workingJob() {
    static WorkingJob job;
    return job;
}

}  // namespace mini_lexical_search
