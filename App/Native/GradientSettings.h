#pragma once
#include <iiSharedCanvasNative.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace congregation::native {
class GradientSettings final {
public:
    GradientSettings();
    const std::vector<iiSharedCanvas::GradientStop>& stops() const noexcept { return stops_; }
    std::size_t selected() const noexcept { return selected_; }
    iiSharedCanvas::GradientKind kind() const noexcept { return kind_; }
    iiSharedCanvas::GradientSpread spread() const noexcept { return spread_; }
    RasterBlendMode blend() const noexcept { return blend_; }
    bool select(std::size_t);
    bool add();
    bool remove();
    bool set_position(double);
    bool set_color(std::uint32_t);
    bool set_alpha(double);
    void set_default_start(std::uint32_t);
    void set_radial(bool);
    void set_repeat(bool);
    void cycle_blend();
    ii::core::Observable<std::uint64_t> revision{0};
private:
    void notify();
    std::vector<iiSharedCanvas::GradientStop> stops_;
    std::size_t selected_ = 0;
    iiSharedCanvas::GradientKind kind_ = iiSharedCanvas::GradientKind::Linear;
    iiSharedCanvas::GradientSpread spread_ = iiSharedCanvas::GradientSpread::Pad;
    RasterBlendMode blend_ = RasterBlendMode::SourceOver;
    bool start_custom_ = false;
};
}
