#pragma once

#include <mutex>
#include <string>

namespace mini_lexical_search {

// 인덱싱 worker와 UI 스레드가 함께 읽고 쓸 수 있다.
class WorkingJob {
public:
    void set(std::string label);
    // 잠금 밖에서 사용할 수 있도록 복사본을 반환한다.
    std::string label() const;

private:
    mutable std::mutex mutex_;
    std::string label_;
};

WorkingJob& workingJob();

}  // namespace mini_lexical_search
