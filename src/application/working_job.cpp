#include "mini_lexical_search/application/working_job.hpp"

#include <utility>

namespace mini_lexical_search {

void WorkingJob::set(std::string label) {
    const std::lock_guard lock(mutex_);
    label_ = std::move(label);
}

std::string WorkingJob::label() const {
    const std::lock_guard lock(mutex_);
    return label_;
}

WorkingJob& workingJob() {
    static WorkingJob job;
    return job;
}

}  // namespace mini_lexical_search
