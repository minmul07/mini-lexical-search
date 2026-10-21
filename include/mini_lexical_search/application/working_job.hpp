#pragma once

#include <string>

namespace mini_lexical_search {

class WorkingJob {
public:
    void set(std::string label);
    const std::string& label() const;

private:
    std::string label_;
};

WorkingJob& workingJob();

}  // namespace mini_lexical_search
