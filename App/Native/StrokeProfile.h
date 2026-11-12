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
        total_ = microseconds > std::numeric_limits<std::uint64_t>::max() - total_
            ? std::numeric_limits<std::uint64_t>::max() : total_ + microseconds;
    }
    [[nodiscard]] std::uint64_t count() const noexcept { return count_; }
    [[nodiscard]] std::uint64_t maximum() const noexcept { return maximum_; }
    [[nodiscard]] std::uint64_t total() const noexcept { return total_; }
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
    std::uint64_t count_ = 0, maximum_ = 0, total_ = 0;
};

struct StrokeStageReport final {
    std::uint64_t count = 0, total_us = 0;
    std::uint64_t p50_upper_us = 0, p95_upper_us = 0, p99_upper_us = 0, maximum_us = 0;
};
struct StrokeRenderSample final {
    std::uint64_t total_us = 0, composition_us = 0, conversion_us = 0, widget_publish_us = 0;
    std::uint64_t composed_tiles = 0, converted_pixels = 0;
};
struct StrokeInputSample final {
    bool captured = false;
    std::uint64_t projection_us = 0, tile_update_us = 0, projected_dabs = 0, projected_samples = 0;
};
struct StrokeProfileReport final {
    bool recorded = false, succeeded = false;
    std::uint64_t begin_us = 0, finish_us = 0, moves = 0, renders = 0;
    std::uint64_t move_p50_upper_us = 0, move_p95_upper_us = 0, move_p99_upper_us = 0, move_max_us = 0;
    std::uint64_t render_p50_upper_us = 0, render_p95_upper_us = 0, render_p99_upper_us = 0, render_max_us = 0;
    // Spacing between input timestamps, not input-to-display latency.
    std::uint64_t max_input_gap_us = 0;
    StrokeStageReport composition, conversion, widget_publish;
    std::uint64_t composed_tiles = 0, converted_pixels = 0;
    StrokeStageReport input_projection, input_tile_update;
    std::uint64_t projected_dabs = 0, projected_samples = 0;
    // A queued presentation is renderer acceptance, not GPU completion or scanout.
    StrokeStageReport presentation_queue;
    std::uint64_t superseded_publications = 0;
    bool pending_presentation = false;
};

class StrokeProfile final {
public:
    explicit StrokeProfile(bool enabled = false) noexcept : enabled_(enabled) {}
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    void begin(std::uint64_t microseconds, double timestamp) noexcept {
        if (!enabled_) return;
        movement_ = {}; rendering_ = {}; composition_ = {}; conversion_ = {}; widget_publish_ = {};
        input_projection_ = {}; input_tile_update_ = {}; presentation_queue_ = {};
        composed_tiles_ = converted_pixels_ = projected_dabs_ = projected_samples_ = 0;
        superseded_publications_ = 0; awaiting_presentation_ = false; render_ready_us_ = 0;
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
        render(StrokeRenderSample{.total_us = microseconds});
    }
    void input(StrokeInputSample sample) noexcept {
        if (!active_ || !sample.captured) return;
        input_projection_.add(sample.projection_us);
        input_tile_update_.add(sample.tile_update_us);
        projected_dabs_ = add_counter(projected_dabs_, sample.projected_dabs);
        projected_samples_ = add_counter(projected_samples_, sample.projected_samples);
    }
    void render(StrokeRenderSample sample) noexcept {
        if (!active_) return;
        rendering_.add(sample.total_us);
        composition_.add(sample.composition_us);
        conversion_.add(sample.conversion_us);
        widget_publish_.add(sample.widget_publish_us);
        composed_tiles_ = add_counter(composed_tiles_, sample.composed_tiles);
        converted_pixels_ = add_counter(converted_pixels_, sample.converted_pixels);
    }
    // Pair the newest widget publication start with the next eligible scene
    // submission. Deferred delivery can leave earlier publications unmatched.
    void render_ready(std::uint64_t timestamp_us) noexcept {
        if (!active_) return;
        if (awaiting_presentation_) superseded_publications_ = add_counter(superseded_publications_, 1);
        render_ready_us_ = timestamp_us;
        awaiting_presentation_ = true;
    }
    void presentation_queued(std::uint64_t timestamp_us) noexcept {
        if (!active_ || !awaiting_presentation_ || timestamp_us < render_ready_us_) return;
        presentation_queue_.add(timestamp_us - render_ready_us_);
        awaiting_presentation_ = false;
    }
    [[nodiscard]] StrokeProfileReport finish(std::uint64_t microseconds, bool succeeded) noexcept {
        if (!active_) return {};
        active_ = false;
        const auto stage = [](const DurationHistogram& histogram) {
            return StrokeStageReport{histogram.count(), histogram.total(), histogram.percentile_upper(50),
                histogram.percentile_upper(95), histogram.percentile_upper(99), histogram.maximum()};
        };
        return {true, succeeded, begin_us_, microseconds, movement_.count(), rendering_.count(),
                movement_.percentile_upper(50), movement_.percentile_upper(95), movement_.percentile_upper(99), movement_.maximum(),
                rendering_.percentile_upper(50), rendering_.percentile_upper(95), rendering_.percentile_upper(99), rendering_.maximum(),
                max_gap_us_, stage(composition_), stage(conversion_), stage(widget_publish_),
                composed_tiles_, converted_pixels_, stage(input_projection_), stage(input_tile_update_),
                projected_dabs_, projected_samples_, stage(presentation_queue_), superseded_publications_,
                awaiting_presentation_};
    }
    void cancel() noexcept { active_ = false; }
private:
    static std::uint64_t add_counter(std::uint64_t left, std::uint64_t right) noexcept {
        return right > std::numeric_limits<std::uint64_t>::max() - left
            ? std::numeric_limits<std::uint64_t>::max() : left + right;
    }
    bool enabled_ = false, active_ = false, has_timestamp_ = false;
    DurationHistogram movement_, rendering_, composition_, conversion_, widget_publish_;
    DurationHistogram input_projection_, input_tile_update_, presentation_queue_;
    std::uint64_t begin_us_ = 0, max_gap_us_ = 0, composed_tiles_ = 0, converted_pixels_ = 0;
    std::uint64_t projected_dabs_ = 0, projected_samples_ = 0;
    std::uint64_t render_ready_us_ = 0, superseded_publications_ = 0;
    bool awaiting_presentation_ = false;
    double timestamp_ = 0;
};
} // namespace congregation::native
