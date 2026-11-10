#pragma once

#include <Stroke/Rasterizer.h>
#include <algorithm>
#include <cmath>
#include <deque>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace congregation::native {

// Only the short, still-changing tip is retained. Earlier dabs become pixels
// immediately; input points and the complete stroke are never stored here.
class StreamingBrush final {
public:
    struct Pixels final {
        std::vector<RasterSample> stable;
        std::vector<RasterSample> transient;
        bool unchanged = false;
    };

    StreamingBrush(BrushState state, double start, double end, double minimum, double strength)
        : state_(std::move(state)), start_(start * state_.rasterizer.brushSize),
          end_(end * state_.rasterizer.brushSize), minimum_(minimum), strength_(strength) {}

    [[nodiscard]] std::expected<Pixels, std::string> append(
        const StrokePoint& sample, bool finish, const RasterSourceSampler& source, DevicePixelRect clip)
    {
        try {
            auto observed = sample;
            // A lift event commonly reports zero pressure after contact has
            // ended. When it has no new position, it is not a new pen sample.
            if (finish && last_ && sample.position.x == last_->position.x &&
                sample.position.y == last_->position.y && !state_.rasterizer.airbrushEnabled) {
                const bool unchangedContact = sample.pressure == last_->pressure &&
                    sample.tiltX == last_->tiltX && sample.tiltY == last_->tiltY &&
                    sample.rotationRadians == last_->rotationRadians &&
                    sample.tangentialPressure == last_->tangentialPressure &&
                    sample.deviceState == last_->deviceState && sample.custom == last_->custom;
                if (sample.pressure == 0 || unchangedContact) observed = *last_;
            }
            auto next = stream_;
            auto produced = appendRasterDabs(next, observed, state_, finish);
            const bool sameSample = last_ && observed.position.x == last_->position.x &&
                observed.position.y == last_->position.y && observed.pressure == last_->pressure &&
                observed.tiltX == last_->tiltX && observed.tiltY == last_->tiltY &&
                observed.rotationRadians == last_->rotationRadians &&
                observed.tangentialPressure == last_->tangentialPressure &&
                observed.deviceState == last_->deviceState && observed.custom == last_->custom;
            const bool usesTime = std::ranges::any_of(state_.dynamics.bindings, [](const auto& binding) {
                return binding.enabled && binding.source == BrushDynamicsSource::Time;
            });
            if (!finish && sameSample && produced.empty() && !state_.rasterizer.airbrushEnabled &&
                (!state_.rasterizer.smoothingEnabled || state_.rasterizer.smoothing <= 0) && !usesTime &&
                next.traveledDistance == stream_.traveledDistance) {
                stream_ = next;
                last_ = observed;
                Pixels unchanged; unchanged.unchanged = true; return unchanged;
            }
            for (auto& dab : produced) pending_.push_back(std::move(dab));
            stream_ = next;

            // Render what the finish operation would currently produce. It is
            // only an overlay: the real stream and undo history never receive
            // the synthetic finish until an actual pointer-up arrives.
            auto predicted = stream_;
            auto endDabs = finish ? std::vector<BrushDab>{} :
                appendRasterDabs(predicted, observed, state_, true);
            const double visibleEnd = predicted.traveledDistance;
            Pixels output;
            const auto emit = [&](BrushDab dab, double distanceToEnd,
                                  std::vector<RasterSample>& destination) {
                const auto smooth = [](double value) {
                    const double t = std::clamp(value, 0.0, 1.0);
                    return t * t * (3.0 - 2.0 * t);
                };
                double progress = 1.0;
                if (start_ > 0) progress = std::min(progress, smooth(dab.strokeDistance / start_));
                if (end_ > 0) progress = std::min(progress, smooth(distanceToEnd / end_));
                dab.scale *= 1.0 - strength_ * (1.0 - minimum_) * (1.0 - progress);
                if (dab.scale <= 0) return;
                auto projected = projectBrushDabs(BrushDabSpan{&dab, 1}, state_.rasterizer,
                                                  RasterProjection{}, source, state_.material);
                std::erase_if(projected, [&](const RasterSample& value) {
                    return value.position.x < clip.origin.x || value.position.y < clip.origin.y ||
                        std::int64_t(value.position.x) >= std::int64_t(clip.origin.x) + clip.width ||
                        std::int64_t(value.position.y) >= std::int64_t(clip.origin.y) + clip.height;
                });
                destination.insert(destination.end(), projected.begin(), projected.end());
            };
            // Bound both command count and an estimate of the projected tip
            // pixels. Otherwise a stationary large airbrush could retain a
            // small number of enormous tips and exhaust memory each frame.
            constexpr std::size_t maximumTipPixels = 1048576;
            const auto diameter = std::max(1.0, state_.rasterizer.brushSize + 4.0);
            const auto estimatedPixels = std::min<double>(maximumTipPixels, diameter * diameter);
            const auto maximumTailDabs = std::min<std::size_t>(16384,
                std::max<std::size_t>(1, std::size_t(maximumTipPixels / estimatedPixels)));
            while (!pending_.empty() &&
                   (pending_.front().strokeDistance + end_ <= stream_.traveledDistance ||
                    pending_.size() > maximumTailDabs)) {
                auto dab = std::move(pending_.front()); pending_.pop_front();
                emit(dab, visibleEnd - dab.strokeDistance, output.stable);
            }
            for (const auto& dab : pending_)
                emit(dab, visibleEnd - dab.strokeDistance, output.transient);
            for (const auto& dab : endDabs)
                emit(dab, visibleEnd - dab.strokeDistance, output.transient);
            if (!finish) last_ = observed;
            return output;
        } catch (const std::exception& error) {
            return std::unexpected(error.what());
        }
    }

private:
    BrushState state_;
    RasterDabStream stream_;
    std::deque<BrushDab> pending_;
    std::optional<StrokePoint> last_;
    double start_, end_, minimum_, strength_;
};

} // namespace congregation::native
