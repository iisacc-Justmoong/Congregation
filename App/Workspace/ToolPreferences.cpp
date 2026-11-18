#include "ToolPreferences.h"
#include "EditorBackend.h"
#include <Shared/Data.h>
#include <iiFileProvider>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace congregation::ui {
namespace {
using namespace ii::network::wire;
using native::GradientSettings;
using native::ToolSettings;
using Bool = ii::core::Observable<bool> EditorBackend::*;
using Real = ii::core::Observable<double> EditorBackend::*;
using Integer = ii::core::Observable<std::int64_t> EditorBackend::*;
struct BoolField { const char* key; Bool member; };
struct RealField { const char* key; Real member; double minimum, maximum; };
struct IntegerField { const char* key; Integer member; std::int64_t minimum, maximum; };
constexpr std::array bool_fields{
    BoolField{"selection_antialias", &EditorBackend::selection_antialias},
    BoolField{"wand_contiguous", &EditorBackend::wand_contiguous},
    BoolField{"wand_current_layer", &EditorBackend::wand_current_layer},
    BoolField{"fill_contiguous", &EditorBackend::fill_contiguous},
    BoolField{"fill_current_layer", &EditorBackend::fill_current_layer},
    BoolField{"fill_target_current", &EditorBackend::fill_target_current},
    BoolField{"fill_preserve_alpha", &EditorBackend::fill_preserve_alpha},
    BoolField{"fill_antialias", &EditorBackend::fill_antialias},
    BoolField{"clone_aligned", &EditorBackend::clone_aligned},
    BoolField{"clone_current_layer", &EditorBackend::clone_current_layer},
    BoolField{"crop_destructive", &EditorBackend::crop_destructive},
    BoolField{"polygon_center", &EditorBackend::polygon_center},
    BoolField{"polygon_fill", &EditorBackend::polygon_fill},
    BoolField{"polygon_outline", &EditorBackend::polygon_outline},
    BoolField{"rulers_visible", &EditorBackend::rulers_visible}};
constexpr std::array real_fields{
    RealField{"selection_feather", &EditorBackend::selection_feather, 0, 32},
    RealField{"magnetic_radius", &EditorBackend::magnetic_radius, 2, 64},
    RealField{"wand_tolerance", &EditorBackend::wand_tolerance, 0, 1},
    RealField{"fill_tolerance", &EditorBackend::fill_tolerance, 0, 1},
    RealField{"polygon_rotation", &EditorBackend::polygon_rotation, -180, 180},
    RealField{"polygon_stroke_width", &EditorBackend::polygon_stroke_width, 0, 64},
    RealField{"polygon_opacity", &EditorBackend::polygon_opacity, 0, 1}};
constexpr std::array integer_fields{
    IntegerField{"crop_aspect", &EditorBackend::crop_aspect, 0, 3},
    IntegerField{"polygon_sides", &EditorBackend::polygon_sides, 3, 64}};

constexpr std::string_view magic = "CongregationToolPreferences\x01";
constexpr std::size_t metadata_limit = 1024 * 1024;
constexpr std::size_t file_limit = magic.size() + 4 + metadata_limit +
    ToolSettings::profile_count * std::size_t(4096) * 4096;
constexpr std::size_t legacy_limit = 131072;

Object save_panel(const EditorBackend& backend) {
    Object panel;
    for (const auto& item : bool_fields) panel.emplace(item.key, Value((backend.*item.member).get()));
    for (const auto& item : real_fields) {
        const auto value=(backend.*item.member).get();
        if (!std::isfinite(value) || value<item.minimum || value>item.maximum)
            throw std::runtime_error("Invalid tool panel value");
        panel.emplace(item.key, Value(value));
    }
    for (const auto& item : integer_fields) {
        const auto value=(backend.*item.member).get();
        if (value<item.minimum || value>item.maximum) throw std::runtime_error("Invalid tool panel value");
        panel.emplace(item.key, Value(value));
    }
    return panel;
}
bool read_real(const Value& value, double minimum, double maximum, double& out) {
    const auto n = number(value, std::numeric_limits<double>::quiet_NaN());
    if (!std::isfinite(n) || n < minimum || n > maximum) return false;
    out = n; return true;
}
bool read_integer(const Value& value, std::int64_t minimum, std::int64_t maximum, std::int64_t& out) {
    double n;
    if (!read_real(value, static_cast<double>(minimum), static_cast<double>(maximum), n) || std::trunc(n)!=n) return false;
    out = static_cast<std::int64_t>(n); return true;
}
bool apply_panel(const Value& value, EditorBackend& backend, bool apply) {
    const auto object = value.get<Object>();
    if (!object) return false;
    for (const auto& item : bool_fields) {
        auto v=field(object->get(), item.key).get<bool>();
        if (!v) return false;
    }
    for (const auto& item : real_fields) {
        double v;
        if (!read_real(field(object->get(), item.key), item.minimum, item.maximum, v)) return false;
    }
    for (const auto& item : integer_fields) {
        std::int64_t v;
        if (!read_integer(field(object->get(), item.key), item.minimum, item.maximum, v)) return false;
    }
    if (apply) {
        for (const auto& item : bool_fields) (backend.*item.member).set(field(object->get(), item.key).get<bool>()->get());
        for (const auto& item : real_fields) (backend.*item.member).set(number(field(object->get(), item.key)));
        for (const auto& item : integer_fields) (backend.*item.member).set(static_cast<std::int64_t>(number(field(object->get(), item.key))));
    }
    return true;
}
Value save_gradient(const GradientSettings::State& gradient) {
    Array stops;
    for (const auto& stop : gradient.stops)
        stops.emplace_back(Object{{"position", Value(stop.position)}, {"argb", Value(std::int64_t(stop.argb))}});
    return Value(Object{{"stops", Value(std::move(stops))}, {"selected", Value(std::int64_t(gradient.selected))},
        {"kind", Value(std::int64_t(gradient.kind))}, {"spread", Value(std::int64_t(gradient.spread))},
        {"blend", Value(std::int64_t(gradient.blend))}, {"start_custom", Value(gradient.start_custom)}});
}
bool read_gradient(const Value& value, GradientSettings::State& result) {
    const auto object=value.get<Object>();
    const auto stops=field(value, "stops").get<Array>();
    if (!object || !stops || stops->get().size()<2 || stops->get().size()>256) return false;
    for (const auto& stop : stops->get()) {
        double position; std::int64_t argb;
        if (!stop.get<Object>() || !read_real(field(stop,"position"),0,1,position) ||
            !read_integer(field(stop,"argb"),0,0xffffffffLL,argb)) return false;
        result.stops.push_back({position, static_cast<std::uint32_t>(argb)});
    }
    std::int64_t selected, kind, spread, blend;
    const auto custom=field(value,"start_custom").get<bool>();
    if (!custom || !read_integer(field(value,"selected"),0,255,selected) ||
        !read_integer(field(value,"kind"),0,1,kind) || !read_integer(field(value,"spread"),0,1,spread) ||
        !read_integer(field(value,"blend"),0,static_cast<int>(RasterBlendMode::Overlay),blend)) return false;
    result.selected=static_cast<std::size_t>(selected);
    result.kind=static_cast<iiSharedCanvas::GradientKind>(kind);
    result.spread=static_cast<iiSharedCanvas::GradientSpread>(spread);
    result.blend=static_cast<RasterBlendMode>(blend);
    result.start_custom=custom->get();
    GradientSettings validated;
    return validated.restore_persistent_state(result);
}

std::string encode(const EditorBackend& backend) {
    auto brush=backend.parameters->persistent_state();
    GradientSettings validatedGradient;
    const auto gradient=backend.gradient_settings->persistent_state();
    auto panel=save_panel(backend);
    if (!ToolSettings::valid_persistent_state(brush) ||
        !validatedGradient.restore_persistent_state(gradient))
        throw std::runtime_error("Invalid tool settings");
    Array textures;
    for (const auto& texture : brush.textures)
        textures.emplace_back(Object{{"width", Value(std::int64_t(texture.width))},
            {"height", Value(std::int64_t(texture.height))}, {"name", Value(texture.name)}});
    const auto color=backend.color->current();
    for (const auto value : {color.red,color.green,color.blue,color.alpha})
        if (!std::isfinite(value) || value<0 || value>1) throw std::runtime_error("Invalid foreground color");
    Array foreground{Value(color.red), Value(color.green), Value(color.blue), Value(color.alpha)};
    const auto meta=json(Value(Object{{"version", Value(std::int64_t(1))}, {"brush", Value(brush.values)},
        {"background", Value(std::int64_t(brush.background))}, {"textures", Value(std::move(textures))},
        {"foreground", Value(std::move(foreground))}, {"panel", Value(std::move(panel))},
        {"gradient", save_gradient(gradient)}}), metadata_limit);
    if (meta.empty() || meta.size()>metadata_limit) throw std::runtime_error("Tool settings metadata is too large");
    std::string result(magic);
    std::size_t total=magic.size()+4+meta.size();
    for (const auto& texture : brush.textures) total+=texture.alpha.size();
    if (total>file_limit) throw std::runtime_error("Tool settings are too large");
    result.reserve(total);
    append_be(result, meta.size(), 4);
    result += meta;
    for (const auto& texture : brush.textures)
        if (!texture.alpha.empty()) result.append(reinterpret_cast<const char*>(texture.alpha.data()), texture.alpha.size());
    if (result.size()>file_limit) throw std::runtime_error("Tool settings are too large");
    return result;
}

bool decode(std::string_view bytes, EditorBackend& backend) {
    if (!bytes.starts_with(magic)) return false;
    bytes.remove_prefix(magic.size());
    std::uint64_t metadataSize=0;
    if (!take_be(bytes,metadataSize,4) || metadataSize>metadata_limit || metadataSize>bytes.size()) return false;
    const auto root=parse_json(bytes.substr(0,metadataSize), metadata_limit);
    bytes.remove_prefix(static_cast<std::size_t>(metadataSize));
    if (!root || !root->get<Object>()) return false;
    std::int64_t version, background;
    const auto brushText=field(*root,"brush").get<std::string>();
    const auto textures=field(*root,"textures").get<Array>();
    const auto foreground=field(*root,"foreground").get<Array>();
    if (!read_integer(field(*root,"version"),1,1,version) ||
        !read_integer(field(*root,"background"),0,0xffffffffLL,background) ||
        !brushText || !textures || textures->get().size()!=ToolSettings::profile_count ||
        !foreground || foreground->get().size()!=4) return false;
    std::array<double,4> channels{};
    for (std::size_t i=0;i<channels.size();++i)
        if (!read_real(foreground->get()[i],0,1,channels[i])) return false;
    ToolSettings::PersistentState brush;
    brush.values=brushText->get(); brush.background=static_cast<std::uint32_t>(background);
    for (std::size_t i=0; i<brush.textures.size(); ++i) {
        const auto& texture=textures->get()[i];
        std::int64_t width,height;
        const auto name=field(texture,"name").get<std::string>();
        if (!texture.get<Object>() || !name || name->get().size()>4096 ||
            !read_integer(field(texture,"width"),0,4096,width) ||
            !read_integer(field(texture,"height"),0,4096,height) || ((width==0)!=(height==0))) return false;
        const auto length=std::size_t(width)*std::size_t(height);
        if (length>bytes.size()) return false;
        auto& target=brush.textures[i];
        target.width=static_cast<int>(width); target.height=static_cast<int>(height); target.name=name->get();
        target.alpha.assign(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(length));
        bytes.remove_prefix(length);
    }
    GradientSettings::State gradient;
    if (!bytes.empty() || !ToolSettings::valid_persistent_state(brush) ||
        !read_gradient(field(*root,"gradient"),gradient) || !apply_panel(field(*root,"panel"),backend,false)) return false;
    if (!backend.set_current_color({channels[0],channels[1],channels[2],channels[3]})) return false;
    if (!backend.parameters->restore_persistent_state(brush) || !backend.gradient_settings->restore_persistent_state(gradient)) return false;
    (void)apply_panel(field(*root,"panel"),backend,true);
    backend.sync_parameter_fields();
    return true;
}
}

ToolPreferences::ToolPreferences(std::filesystem::path directory) : directory_(std::move(directory)) {}
std::expected<void, std::string> ToolPreferences::save(const EditorBackend& backend) const {
    try {
        if (directory_.empty()) return std::unexpected("App storage is unavailable");
        const auto data=encode(backend);
        iiFileProvider::native::File::createDirectories(directory_);
        iiFileProvider::native::StagedFile stage(directory_/"tool-preferences.bin");
        iiFileProvider::native::File::write(stage.path(),data);
        const auto published=stage.publish();
        if (!published.succeeded) return std::unexpected(published.message);
        return {};
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
std::expected<bool, std::string> ToolPreferences::load(EditorBackend& backend) const {
    try {
        if (directory_.empty()) return std::unexpected("App storage is unavailable");
        std::error_code error;
        const auto path=directory_/"tool-preferences.bin";
        const auto exists=std::filesystem::exists(path,error);
        if (error) return std::unexpected(error.message());
        if (exists) {
            const auto bytes=iiFileProvider::native::File::read(path,file_limit);
            if (!decode(bytes,backend)) return std::unexpected("Saved tool settings are invalid or incomplete");
            return true;
        }
        const auto legacy=directory_/"tools.ini";
        const auto hasLegacy=std::filesystem::exists(legacy,error);
        if (error) return std::unexpected(error.message());
        if (!hasLegacy) return false;
        const auto bytes=iiFileProvider::native::File::read(legacy,legacy_limit);
        if (!backend.parameters->restore(bytes)) return std::unexpected("Saved legacy tool settings are invalid");
        backend.sync_parameter_fields();
        return true;
    } catch (const std::exception& error) { return std::unexpected(error.what()); }
}
}
