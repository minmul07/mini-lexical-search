#include "mini_lexical_search/presentation/tui/index_progress.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

#include "mini_lexical_search/application/log.hpp"

namespace mini_lexical_search::tui {
namespace {

constexpr int kHomeMaxInnerWidth = 100;
constexpr int kHomeBorderColumns = 2;
constexpr float kMetadataEnd = 0.25F;
constexpr float kTokenizationEnd = 0.90F;
constexpr float kSaveEnd = 0.99F;
constexpr std::size_t kMaxLines = 10;

float ratio(std::size_t completed, std::size_t total) {
    if (total == 0) return 0.0F;
    return std::clamp(static_cast<float>(completed) / static_cast<float>(total), 0.0F, 1.0F);
}

class FillNode : public ftxui::Node {
public:
    FillNode(ftxui::Element child, float progress, float opacity)
        : progress_(std::clamp(progress, 0.0F, 1.0F)),
          opacity_(std::clamp(opacity, 0.0F, 1.0F)),
          color_(opacity_ >= 1.0F
                     ? ftxui::Color::RGB(64, 64, 64)
                     : ftxui::Color::Interpolate(1.0F - opacity_,
                                                 ftxui::Color::RGB(64, 64, 64),
                                                 ftxui::Color::RGB(0, 0, 0))) {
        children_.push_back(std::move(child));
    }

    void ComputeRequirement() override {
        children_.front()->ComputeRequirement();
        requirement_ = children_.front()->requirement();
    }

    void SetBox(ftxui::Box box) override {
        box_ = box;
        children_.front()->SetBox(box);
    }

    void Check(Status* status) override {
        children_.front()->Check(status);
    }

    void Select(ftxui::Selection& selection) override {
        children_.front()->Select(selection);
    }

    std::string GetSelectedContent(ftxui::Selection& selection) override {
        return children_.front()->GetSelectedContent(selection);
    }

    void Render(ftxui::Screen& screen) override {
        children_.front()->Render(screen);
        const int width = box_.x_max - box_.x_min + 1;
        if (width <= 0 || box_.y_max < box_.y_min || opacity_ <= 0.0F) return;
        const int filled = std::clamp(static_cast<int>(progress_ * static_cast<float>(width) + 0.5F),
                                      0, width);
        for (int y = box_.y_min; y <= box_.y_max; ++y) {
            for (int x = box_.x_min; x < box_.x_min + filled; ++x) {
                auto& cell = screen.CellAt(x, y);
                cell.background_color = color_;
                if (cell.character.empty()) cell.character = " ";
            }
        }
    }

private:
    float progress_;
    float opacity_;
    ftxui::Color color_;
};

}  // namespace

int homeFieldWidth(int terminalWidth) {
    return std::max(1, std::min(kHomeMaxInnerWidth, terminalWidth - kHomeBorderColumns));
}

HomeGeometry homeGeometry(int terminalWidth, int terminalHeight, int inputHeight) {
    HomeGeometry geometry;
    geometry.fieldWidth = homeFieldWidth(terminalWidth);
    geometry.fieldLeft = std::max(0, (terminalWidth - geometry.fieldWidth) / 2);
    geometry.boxHeight = std::max(1, inputHeight) + kHomeBorderColumns;
    geometry.boxTop = std::max(0, (std::max(1, terminalHeight) - geometry.boxHeight) / 2);
    geometry.detailTop = geometry.boxTop + geometry.boxHeight + 1;
    geometry.detailHeight = std::max(0, terminalHeight - geometry.detailTop);
    return geometry;
}

ftxui::Element progressFill(ftxui::Element child, float progress, float opacity) {
    return std::make_shared<FillNode>(std::move(child), progress, opacity);
}

void IndexProgressModel::begin() {
    const std::lock_guard lock(mutex_);
    active_ = true;
    finished_ = false;
    failed_ = false;
    target_ = 0.02F;
    error_.clear();
    lines_.clear();
}

void IndexProgressModel::clear() {
    const std::lock_guard lock(mutex_);
    active_ = false;
    finished_ = false;
    failed_ = false;
    target_ = 0.0F;
    error_.clear();
    lines_.clear();
}

void IndexProgressModel::replaceLine(const std::string& prefix, std::string line) {
    for (auto it = lines_.rbegin(); it != lines_.rend(); ++it) {
        if (it->rfind(prefix, 0) == 0) {
            *it = std::move(line);
            return;
        }
    }
    lines_.push_back(std::move(line));
}

void IndexProgressModel::apply(const IndexProgress& event) {
    const std::lock_guard lock(mutex_);
    std::ostringstream line;
    switch (event.stage) {
    case IndexStage::MetadataStarted:
        target_ = 0.02F;
        replaceLine("updating file metadata..", "updating file metadata..");
        break;
    case IndexStage::MetadataProgress:
        target_ = 0.02F + (kMetadataEnd - 0.02F) * ratio(event.completed, event.total);
        line << "updating file metadata..";
        if (event.total > 0) line << " (" << event.completed << '/' << event.total << ')';
        replaceLine("updating file metadata..", line.str());
        break;
    case IndexStage::MetadataCompleted:
        target_ = kMetadataEnd;
        line << "updated file metadata ("
             << formatElapsedMilliseconds(event.elapsedMicroseconds) << " ms)";
        replaceLine("updating file metadata..", line.str());
        line.str({});
        line.clear();
        line << event.metadata.notUpdated << " not updated, " << event.metadata.updated
             << " updated, " << event.metadata.added << " added, "
             << event.metadata.deleted << " deleted";
        lines_.push_back(line.str());
        break;
    case IndexStage::TokenizationStarted:
        target_ = kMetadataEnd;
        line << "tokenizing documents..";
        replaceLine("tokenizing documents..", line.str());
        break;
    case IndexStage::TokenizationProgress:
        target_ = kMetadataEnd + (kTokenizationEnd - kMetadataEnd) *
                                     ratio(event.completed, event.total);
        line << "tokenizing documents..";
        if (event.total > 0) line << " (" << event.completed << '/' << event.total << ')';
        replaceLine("tokenizing documents..", line.str());
        break;
    case IndexStage::TokenizationCompleted:
        target_ = kTokenizationEnd;
        line << "tokenized " << event.tokens.documents << " documents ("
             << formatElapsedMilliseconds(event.elapsedMicroseconds) << " ms)";
        replaceLine("tokenizing documents..", line.str());
        break;
    case IndexStage::SaveStarted:
        target_ = 0.93F;
        replaceLine("saving index..", "saving index..");
        break;
    case IndexStage::SaveCompleted:
        target_ = kSaveEnd;
        line << "saved index (" << formatElapsedMilliseconds(event.elapsedMicroseconds) << " ms)";
        replaceLine("saving index..", line.str());
        break;
    }
    if (lines_.size() > kMaxLines) {
        lines_.erase(lines_.begin(), lines_.begin() + static_cast<std::ptrdiff_t>(lines_.size() - kMaxLines));
    }
}

void IndexProgressModel::finish(const IndexUpdate&) {
    const std::lock_guard lock(mutex_);
    active_ = false;
    finished_ = true;
    failed_ = false;
    target_ = 1.0F;
}

void IndexProgressModel::fail(std::string message) {
    const std::lock_guard lock(mutex_);
    active_ = false;
    finished_ = true;
    failed_ = true;
    error_ = message;
    lines_.push_back("error: indexing failed: " + message);
    if (lines_.size() > kMaxLines) {
        lines_.erase(lines_.begin(), lines_.begin() + static_cast<std::ptrdiff_t>(lines_.size() - kMaxLines));
    }
}

IndexDisplay IndexProgressModel::display() const {
    const std::lock_guard lock(mutex_);
    return {active_, finished_, failed_, target_, error_, lines_};
}

}  // namespace mini_lexical_search::tui
