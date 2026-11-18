#pragma once
#include <cstddef>
#include <deque>
#include <string>
#include <string_view>

namespace congregation {
// Only one optimistic edit is in flight. Later edits wait for its exact echo.
class LocalChangeQueue final {
public:
    explicit LocalChangeQueue(std::size_t byte_limit = 512ULL * 1024 * 1024) : limit_(byte_limit) {}
    bool push(std::string payload) {
        if (payload.empty() || entries_.size() >= 4096 || payload.size() > limit_ || bytes_ > limit_ - payload.size())
            return false;
        const auto size = payload.size();
        entries_.push_back(std::move(payload));
        bytes_ += size;
        return true;
    }
    [[nodiscard]] bool ready() const noexcept { return !entries_.empty() && !sent_; }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& front() const noexcept { return entries_.front(); }
    bool mark_sent() noexcept { if (!ready()) return false; sent_ = true; return true; }
    bool acknowledge(std::string_view payload) noexcept {
        if (!sent_ || entries_.empty() || entries_.front() != payload) return false;
        bytes_ -= entries_.front().size();
        entries_.pop_front();
        sent_ = false;
        return true;
    }
    void clear() noexcept { entries_.clear(); bytes_ = 0; sent_ = false; }
private:
    std::deque<std::string> entries_;
    std::size_t bytes_ = 0, limit_;
    bool sent_ = false;
};
}
