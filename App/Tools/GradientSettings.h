#pragma once
#include <iiSharedCanvas>
#include <iiSDKCore>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <cstdint>
#include <vector>

namespace congregation::native {
class GradientSettings final : public std::enable_shared_from_this<GradientSettings> {
public:
    enum class Field { content, selection, position, color, alpha, kind, spread, blend, total };
    struct State final {
        std::vector<iiSharedCanvas::GradientStop> stops;
        std::size_t selected = 0;
        iiSharedCanvas::GradientKind kind = iiSharedCanvas::GradientKind::Linear;
        iiSharedCanvas::GradientSpread spread = iiSharedCanvas::GradientSpread::Pad;
        RasterBlendMode blend = RasterBlendMode::SourceOver;
        bool start_custom = false;
    };
    GradientSettings();
    [[nodiscard]] State persistent_state() const;
    bool restore_persistent_state(const State&);
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
    [[nodiscard]] const ii::core::Observable<std::uint64_t>& field_revision(Field field) const;
private:
    void notify(std::initializer_list<Field>);
    void notify(std::span<const Field>);
    std::array<ii::core::Observable<std::uint64_t>, static_cast<std::size_t>(Field::total)> field_revisions_;
    std::vector<iiSharedCanvas::GradientStop> stops_;
    std::size_t selected_ = 0;
    iiSharedCanvas::GradientKind kind_ = iiSharedCanvas::GradientKind::Linear;
    iiSharedCanvas::GradientSpread spread_ = iiSharedCanvas::GradientSpread::Pad;
    RasterBlendMode blend_ = RasterBlendMode::SourceOver;
    bool start_custom_ = false;
};
}
