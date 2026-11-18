#include "GradientSettings.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace congregation::native {
using namespace iiSharedCanvas;
GradientSettings::GradientSettings():stops_{{0,0xff000000U},{1,0x00000000U}}{}
GradientSettings::State GradientSettings::persistent_state() const {
    return {stops_, selected_, kind_, spread_, blend_, start_custom_};
}
bool GradientSettings::restore_persistent_state(const State& state) {
    if (state.stops.size()<2 || state.stops.size()>256 || state.selected>=state.stops.size() ||
        state.stops.front().position!=0 || state.stops.back().position!=1 ||
        (state.kind!=GradientKind::Linear && state.kind!=GradientKind::Radial) ||
        (state.spread!=GradientSpread::Pad && state.spread!=GradientSpread::Repeat) ||
        (state.blend!=RasterBlendMode::SourceOver && state.blend!=RasterBlendMode::Multiply &&
         state.blend!=RasterBlendMode::Screen && state.blend!=RasterBlendMode::Overlay)) return false;
    for (std::size_t i=0;i<state.stops.size();++i)
        if (!std::isfinite(state.stops[i].position) || state.stops[i].position<0 || state.stops[i].position>1 ||
            (i && state.stops[i-1].position>=state.stops[i].position)) return false;
    const auto oldStop = stops_[selected_];
    const auto newStop = state.stops[state.selected];
    const bool selection = selected_ != state.selected || stops_.size() != state.stops.size();
    const bool position = oldStop.position != newStop.position;
    const bool color = (oldStop.argb & 0xffffffU) != (newStop.argb & 0xffffffU);
    const bool alpha = (oldStop.argb >> 24) != (newStop.argb >> 24);
    const bool kind = kind_ != state.kind, spread = spread_ != state.spread, blend = blend_ != state.blend;
    const bool content = stops_.size() != state.stops.size() ||
        !std::equal(stops_.begin(), stops_.end(), state.stops.begin(), state.stops.end(),
                    [](const auto& a, const auto& b) { return a.position==b.position && a.argb==b.argb; });
    const bool changed = selection || position || color || alpha || kind || spread || blend ||
                         content || start_custom_ != state.start_custom;
    if (!changed) return true;
    stops_=state.stops; selected_=state.selected; kind_=state.kind; spread_=state.spread;
    blend_=state.blend; start_custom_=state.start_custom;
    std::vector<Field> fields;
    if (content) fields.push_back(Field::content);
    if (selection) fields.push_back(Field::selection);
    if (position) fields.push_back(Field::position);
    if (color) fields.push_back(Field::color);
    if (alpha) fields.push_back(Field::alpha);
    if (kind) fields.push_back(Field::kind);
    if (spread) fields.push_back(Field::spread);
    if (blend) fields.push_back(Field::blend);
    notify(fields);
    return true;
}
const ii::core::Observable<std::uint64_t>& GradientSettings::field_revision(Field field) const {
    const auto index = static_cast<std::size_t>(field);
    if (index >= field_revisions_.size()) throw std::out_of_range("Unknown gradient setting");
    return field_revisions_[index];
}
void GradientSettings::notify(std::span<const Field> fields) {
    const auto keep_alive = weak_from_this().lock();
    std::exception_ptr failure;
    for (const auto field : fields) {
        auto& signal = field_revisions_[static_cast<std::size_t>(field)];
        try { signal.set(signal.get()+1); }
        catch (...) { if (!failure) failure=std::current_exception(); }
    }
    try { revision.set(revision.get()+1); }
    catch (...) { if (!failure) failure=std::current_exception(); }
    if (failure) std::rethrow_exception(failure);
}
void GradientSettings::notify(std::initializer_list<Field> fields) {
    notify(std::span<const Field>(fields.begin(), fields.size()));
}
bool GradientSettings::select(std::size_t index)
{
    if (index >= stops_.size()) return false;
    if (selected_ != index) { selected_ = index; notify({Field::selection, Field::position, Field::color, Field::alpha}); }
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
    selected_ = left+1; notify({Field::content, Field::selection, Field::position, Field::color, Field::alpha}); return true;
}
bool GradientSettings::remove()
{
    if (selected_ == 0 || selected_ + 1 >= stops_.size()) return false;
    stops_.erase(stops_.begin() + std::ptrdiff_t(selected_));
    selected_ = std::min(selected_, stops_.size()-1); notify({Field::content, Field::selection, Field::position, Field::color, Field::alpha}); return true;
}
bool GradientSettings::set_position(double position)
{
    if (!std::isfinite(position) || selected_ == 0 || selected_ + 1 >= stops_.size()) return false;
    const auto low = stops_[selected_-1].position + 1e-6, high = stops_[selected_+1].position - 1e-6;
    if (low > high) return false;
    position = std::clamp(position, low, high);
    if (stops_[selected_].position != position) { stops_[selected_].position = position; notify({Field::content, Field::position}); }
    return true;
}
bool GradientSettings::set_color(std::uint32_t color)
{
    if (selected_ == 0) start_custom_ = true;
    if (stops_[selected_].argb != color) {
        const auto before = stops_[selected_].argb;
        stops_[selected_].argb = color;
        if ((before & 0xffffffU) != (color & 0xffffffU) && (before >> 24) != (color >> 24))
            notify({Field::content, Field::color, Field::alpha});
        else if ((before & 0xffffffU) != (color & 0xffffffU)) notify({Field::content, Field::color});
        else notify({Field::content, Field::alpha});
    }
    return true;
}
bool GradientSettings::set_alpha(double alpha)
{
    if (!std::isfinite(alpha) || alpha < 0 || alpha > 1) return false;
    return set_color((stops_[selected_].argb & 0xffffffU) | (std::uint32_t(std::lround(alpha*255)) << 24));
}
void GradientSettings::set_default_start(std::uint32_t color)
{
    if (!start_custom_ && stops_[0].argb != color) {
        const auto before=stops_[0].argb; stops_[0].argb=color;
        if (selected_ != 0) notify({Field::content});
        else if ((before & 0xffffffU)!=(color & 0xffffffU) && (before >> 24)!=(color >> 24))
            notify({Field::content, Field::color, Field::alpha});
        else if ((before & 0xffffffU)!=(color & 0xffffffU)) notify({Field::content, Field::color});
        else notify({Field::content, Field::alpha});
    }
}
void GradientSettings::set_radial(bool radial)
{
    const auto kind = radial ? GradientKind::Radial : GradientKind::Linear;
    if (kind_ != kind) { kind_ = kind; notify({Field::kind}); }
}
void GradientSettings::set_repeat(bool repeat)
{
    const auto spread = repeat ? GradientSpread::Repeat : GradientSpread::Pad;
    if (spread_ != spread) { spread_ = spread; notify({Field::spread}); }
}
void GradientSettings::cycle_blend()
{
    blend_ = blend_ == RasterBlendMode::SourceOver ? RasterBlendMode::Multiply
           : blend_ == RasterBlendMode::Multiply ? RasterBlendMode::Screen
           : blend_ == RasterBlendMode::Screen ? RasterBlendMode::Overlay : RasterBlendMode::SourceOver;
    notify({Field::blend});
}
}
