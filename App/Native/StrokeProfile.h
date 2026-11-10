#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace congregation::native {

// Bounded, allocation-free timing summary. Percentiles are upper bounds of
// power-of-two microsecond buckets, not individual input-to-display timings.
class DurationHistogram final {
public:
    void add(std::uint64_t microseconds) noexcept {
        const auto bucket = std::bit_width(microseconds);
        ++buckets_[bucket];
        ++count_;
        maximum_ = std::max(maximum_, microseconds);
    }
    [[nodiscard]] std::uint64_t count() const noexcept { return count_; }
    [[nodiscard]] std::uint64_t maximum() const noexcept { return maximum_; }
    [[nodiscard]] std::uint64_t percentile_upper(unsigned percentile) const noexcept {
        if (!count_) return 0;
        percentile = std::clamp(percentile, 1u, 100u);
        const auto rank = (count_ / 100) * percentile +
            ((count_ % 100) * percentile + 99) / 100;
        std::uint64_t seen = 0;
        for (std::size_t i = 0; i < buckets_.size(); ++i) {
            seen += buckets_[i];
            if (seen >= rank) {
                if (i == 0) return 0;
                if (i == 64) return std::numeric_limits<std::uint64_t>::max();
                return (std::uint64_t{1} << i) - 1;
            }
        }
        return maximum_;
    }
private:
    std::array<std::uint64_t, 65> buckets_{};
    std::uint64_t count_ = 0, maximum_ = 0;
};

struct StrokeProfileReport final {
    bool recorded = false, succeeded = false;
    std::uint64_t begin_us = 0, finish_us = 0, moves = 0, renders = 0;
    std::uint64_t move_p50_upper_us = 0, move_p95_upper_us = 0, move_p99_upper_us = 0, move_max_us = 0;
    std::uint64_t render_p50_upper_us = 0, render_p95_upper_us = 0, render_p99_upper_us = 0, render_max_us = 0;
    // Spacing between input timestamps, not input-to-display latency.
    std::uint64_t max_input_gap_us = 0;
};

class StrokeProfile final {
public:
    explicit StrokeProfile(bool enabled = false) noexcept : enabled_(enabled) {}
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    void begin(std::uint64_t microseconds, double timestamp) noexcept {
        if (!enabled_) return;
        movement_ = {}; rendering_ = {};
        begin_us_ = microseconds; max_gap_us_ = 0;
        has_timestamp_ = std::isfinite(timestamp) && timestamp > 0;
        timestamp_ = has_timestamp_ ? timestamp : 0;
        active_ = true;
    }
    void move(std::uint64_t microseconds, double timestamp) noexcept {
        if (!active_) return;
        movement_.add(microseconds);
        if (!std::isfinite(timestamp) || timestamp <= 0) return;
        if (!has_timestamp_) { timestamp_ = timestamp; has_timestamp_ = true; return; }
        if (timestamp < timestamp_) return;
        const auto gap = (timestamp - timestamp_) * 1000000.;
        if (gap < double(std::numeric_limits<std::uint64_t>::max()))
            max_gap_us_ = std::max(max_gap_us_, std::uint64_t(gap));
        timestamp_ = timestamp;
    }
    void render(std::uint64_t microseconds) noexcept {
        if (active_) rendering_.add(microseconds);
    }
    [[nodiscard]] StrokeProfileReport finish(std::uint64_t microseconds, bool succeeded) noexcept {
        if (!active_) return {};
        active_ = false;
        return {true, succeeded, begin_us_, microseconds, movement_.count(), rendering_.count(),
                movement_.percentile_upper(50), movement_.percentile_upper(95), movement_.percentile_upper(99), movement_.maximum(),
                rendering_.percentile_upper(50), rendering_.percentile_upper(95), rendering_.percentile_upper(99), rendering_.maximum(),
                max_gap_us_};
    }
    void cancel() noexcept { active_ = false; }
private:
    bool enabled_ = false, active_ = false, has_timestamp_ = false;
    DurationHistogram movement_, rendering_;
    std::uint64_t begin_us_ = 0, max_gap_us_ = 0;
    double timestamp_ = 0;
};
} // namespace congregation::native
