#include "GradientSettings.h"
#include <algorithm>
#include <cmath>

namespace congregation::native {
using namespace iiSharedCanvas;
GradientSettings::GradientSettings():stops_{{0,0xff000000U},{1,0x00000000U}}{}
void GradientSettings::notify() { revision.set(revision.get()+1); }
bool GradientSettings::select(std::size_t index)
{
    if (index >= stops_.size()) return false;
    if (selected_ != index) { selected_ = index; notify(); }
    return true;
}
bool GradientSettings::add()
{
    if (stops_.size() >= 256) return false;
    std::size_t left = 0;
    for (std::size_t i = 1; i + 1 < stops_.size(); ++i)
        if (stops_[i+1].position - stops_[i].position > stops_[left+1].position - stops_[left].position) left = i;
    const auto gap = stops_[left+1].position - stops_[left].position;
    if (gap <= 2e-6) return false;
    // Sample with the same premultiplied-alpha interpolation as the renderer so
    // inserting a stop does not unexpectedly change the existing gradient.
    GradientDefinition midpoint;
    midpoint.start = {0, 0}; midpoint.end = {1, 0};
    midpoint.stops = {{0, stops_[left].argb}, {1, stops_[left+1].argb}};
    const auto sampled = gradientRaster({1, 1}, midpoint);
    if (!sampled.ok()) return false;
    const GradientStop stop{stops_[left].position + gap * .5, sampled.pixels.pixels[0]};
    stops_.insert(stops_.begin() + std::ptrdiff_t(left+1), stop);
    selected_ = left+1; notify(); return true;
}
bool GradientSettings::remove()
{
    if (selected_ == 0 || selected_ + 1 >= stops_.size()) return false;
    stops_.erase(stops_.begin() + std::ptrdiff_t(selected_));
    selected_ = std::min(selected_, stops_.size()-1); notify(); return true;
}
bool GradientSettings::set_position(double position)
{
    if (!std::isfinite(position) || selected_ == 0 || selected_ + 1 >= stops_.size()) return false;
    const auto low = stops_[selected_-1].position + 1e-6, high = stops_[selected_+1].position - 1e-6;
    if (low > high) return false;
    position = std::clamp(position, low, high);
    if (stops_[selected_].position != position) { stops_[selected_].position = position; notify(); }
    return true;
}
bool GradientSettings::set_color(std::uint32_t color)
{
    if (selected_ == 0) start_custom_ = true;
    if (stops_[selected_].argb != color) { stops_[selected_].argb = color; notify(); }
    return true;
}
bool GradientSettings::set_alpha(double alpha)
{
    if (!std::isfinite(alpha) || alpha < 0 || alpha > 1) return false;
    return set_color((stops_[selected_].argb & 0xffffffU) | (std::uint32_t(std::lround(alpha*255)) << 24));
}
void GradientSettings::set_default_start(std::uint32_t color)
{
    if (!start_custom_ && stops_[0].argb != color) { stops_[0].argb = color; notify(); }
}
void GradientSettings::set_radial(bool radial)
{
    const auto kind = radial ? GradientKind::Radial : GradientKind::Linear;
    if (kind_ != kind) { kind_ = kind; notify(); }
}
void GradientSettings::set_repeat(bool repeat)
{
    const auto spread = repeat ? GradientSpread::Repeat : GradientSpread::Pad;
    if (spread_ != spread) { spread_ = spread; notify(); }
}
void GradientSettings::cycle_blend()
{
    blend_ = blend_ == RasterBlendMode::SourceOver ? RasterBlendMode::Multiply
           : blend_ == RasterBlendMode::Multiply ? RasterBlendMode::Screen
           : blend_ == RasterBlendMode::Screen ? RasterBlendMode::Overlay : RasterBlendMode::SourceOver;
    notify();
}
}
