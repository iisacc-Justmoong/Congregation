#include "CanvasSetupModel.h"
#include "CanvasCatalogue.generated.h"
#include <algorithm>
#include <cmath>
#include <iiJson/Json.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <limits>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

namespace congregation::native
{
namespace
{
using Value = ii::core::Value;
const Value& field(const Value& value, std::string_view name)
{
    static const Value missing;
    const auto object = value.get<Value::Object>();
    if (!object)
        return missing;
    const auto found = object->get().find(name);
    return found == object->get().end() ? missing : found->second;
}
const Value::Array& array(const Value& value)
{
    const auto result = value.get<Value::Array>();
    if (!result)
        throw std::runtime_error("Canvas catalogue requires an array");
    return result->get();
}
std::string text(const Value& value)
{
    const auto result = value.get<std::string>();
    if (!result)
        throw std::runtime_error("Canvas catalogue requires a string");
    return result->get();
}
double number(const Value& value, double fallback = 0)
{
    return std::visit(
        [fallback](const auto& v) -> double
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
                return double(v);
            return fallback;
        },
        value.storage());
}
std::string lower(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char c)
                           { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : char(c); });
    return value;
}
std::string decimal(double value)
{
    return ii::core::format_decimal(value);
}
double parsed(const std::string& value)
{
    auto begin = value.find_first_not_of(" \t\r\n"), end = value.find_last_not_of(" \t\r\n");
    if (begin == value.npos)
        return 0;
    auto n = ii::core::parse_decimal(std::string_view(value).substr(begin, end - begin + 1));
    return n ? *n : std::numeric_limits<double>::quiet_NaN();
}
double scale(std::string_view unit, double ppi)
{
    return unit == "px" ? 1 : unit == "mm" ? ppi / 25.4 : unit == "in" ? ppi : 0;
}
double nearest_even(double value)
{
    const auto floor = std::floor(value);
    return value - floor == .5 && std::fmod(floor, 2.) == 0 ? floor : std::floor(value + .5);
}
} // namespace
CanvasCatalogue::CanvasCatalogue(std::string_view json)
{
    const auto tree = ii::json::parse(json);
    if (!tree)
        throw std::runtime_error(tree.error().message);
    std::set<std::string> ids;
    for (const auto& category : array(field(*tree, "categories")))
    {
        CanvasCategory item{text(field(category, "name")), text(field(category, "subtitle"))};
        for (const auto& section : array(field(category, "sections")))
            for (const auto& value : array(field(section, "presets")))
            {
                CanvasPreset p;
                p.id = text(field(value, "id"));
                p.name = text(field(value, "name"));
                p.category = item.name;
                p.category_index = categories_.size();
                p.section = text(field(section, "name"));
                p.unit = text(field(value, "unit"));
                p.width = number(field(value, "width"));
                p.height = number(field(value, "height"));
                p.ppi = number(field(value, "ppi"), 300);
                const auto authored_width = number(field(value, "pixelWidth"));
                const auto authored_height = number(field(value, "pixelHeight"));
                auto checked = specification(p.width, p.height, p.unit, p.ppi);
                if (!ids.insert(p.id).second || !checked ||
                    checked->pixel_width != authored_width ||
                    checked->pixel_height != authored_height)
                    throw std::runtime_error("Invalid canvas preset: " + p.id);
                p.pixel_width = checked->pixel_width;
                p.pixel_height = checked->pixel_height;
                presets_.push_back(std::move(p));
                ++item.count;
            }
        categories_.push_back(std::move(item));
    }
    if (categories_.empty() || presets_.empty())
        throw std::runtime_error("Canvas catalogue is empty");
}
const CanvasCatalogue& CanvasCatalogue::bundled()
{
    static const CanvasCatalogue value(data::canvas_catalogue);
    return value;
}
const CanvasPreset* CanvasCatalogue::find(std::string_view id) const
{
    const auto p = std::ranges::find(presets_, id, &CanvasPreset::id);
    return p == presets_.end() ? nullptr : &*p;
}
std::vector<CanvasSection> CanvasCatalogue::sections(std::size_t category,
                                                     std::string_view query) const
{
    auto normalized = lower(std::string(query));
    for (auto at = normalized.find("×"); at != normalized.npos; at = normalized.find("×"))
        normalized.replace(at, 2, " ");
    for (std::size_t i = 0; i < normalized.size(); ++i)
        if (normalized[i] == 'x')
        {
            auto before = normalized.find_last_not_of(" \t", i ? i - 1 : 0);
            auto after = normalized.find_first_not_of(" \t", i + 1);
            if (i && before != normalized.npos && after != normalized.npos &&
                normalized[before] >= '0' && normalized[before] <= '9' &&
                normalized[after] >= '0' && normalized[after] <= '9')
                normalized[i] = ' ';
        }
    std::istringstream input(normalized);
    std::vector<std::string> words;
    for (std::string word; input >> word;)
        words.push_back(std::move(word));
    std::vector<CanvasSection> result;
    std::string previous;
    for (std::size_t i = 0; i < presets_.size(); ++i)
    {
        const auto& p = presets_[i];
        if (words.empty() && p.category_index != category)
            continue;
        const auto haystack =
            lower(p.name + " " + p.category + " " + p.section + " " + decimal(p.width) + " " +
                  decimal(p.height) + " " + p.unit + " " + std::to_string(p.pixel_width) + " " +
                  std::to_string(p.pixel_height));
        if (!std::ranges::all_of(
                words, [&](const auto& word)
                { return word == "x" ? p.section == "X" : haystack.find(word) != haystack.npos; }))
            continue;
        const auto group = p.category + " · " + p.section;
        if (group != previous)
        {
            result.push_back({words.empty() ? p.section : group, {}});
            previous = group;
        }
        result.back().presets.push_back(i);
    }
    return result;
}
std::expected<CanvasDefinition, std::string>
CanvasCatalogue::specification(double width, double height, std::string unit, double ppi,
                               std::string background)
{
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
        return std::unexpected("Enter a positive width and height.");
    if (!std::isfinite(ppi) || ppi <= 0 || ppi > 2400)
        return std::unexpected("Resolution must be between 1 and 2400 PPI.");
    const auto ratio = scale(unit, ppi);
    if (ratio == 0)
        return std::unexpected("Choose px, mm or in.");
    if (unit == "px" && (std::floor(width) != width || std::floor(height) != height))
        return std::unexpected("Pixel dimensions must be whole numbers.");
    const auto w = nearest_even(width * ratio), h = nearest_even(height * ratio);
    if (w < 1 || h < 1 || w > 32768 || h > 32768 || w * h > 268435456)
        return std::unexpected("Use dimensions up to 32768 px and 268 megapixels.");
    if (background != "White" && background != "Transparent" && background != "Black")
        return std::unexpected("Choose a canvas background.");
    return CanvasDefinition{
        width, height, ppi, int(w), int(h), std::move(unit), std::move(background), {}, {}, {}, {}};
}
double CanvasCatalogue::convert(double value, std::string_view from, std::string_view to,
                                double ppi)
{
    const auto source = scale(from, ppi), target = scale(to, ppi);
    return std::isfinite(value) && std::isfinite(ppi) && ppi > 0 && source > 0 && target > 0
               ? value * source / target
               : 0;
}
CanvasSetupModel::CanvasSetupModel()
{
    begin();
}
void CanvasSetupModel::begin()
{
    background = "White";
    choose_category(0);
}
void CanvasSetupModel::select(const CanvasPreset& preset)
{
    selected = &preset;
    custom = false;
    unit = preset.unit;
    new_width.set(decimal(preset.width));
    new_height.set(decimal(preset.height));
    resolution = decimal(preset.ppi);
}
void CanvasSetupModel::choose_category(std::size_t index)
{
    if (index >= catalogue().categories().size())
        return;
    category_index = index;
    query.clear();
    custom = false;
    selected = nullptr;
    const auto groups = sections();
    if (!groups.empty())
        select(catalogue().presets()[groups.front().presets.front()]);
    notify(CanvasSetupChange::gallery);
}
bool CanvasSetupModel::choose(std::string_view id)
{
    const auto* preset = catalogue().find(id);
    if (!preset)
        return false;
    const bool was_custom = custom;
    select(*preset);
    notify(was_custom ? CanvasSetupChange::gallery : CanvasSetupChange::selection);
    return true;
}
void CanvasSetupModel::search(std::string value)
{
    query = std::move(value);
    custom = false;
    selected = nullptr;
    const auto groups = sections();
    if (!groups.empty())
        select(catalogue().presets()[groups.front().presets.front()]);
    notify(CanvasSetupChange::gallery);
}
void CanvasSetupModel::custom_size()
{
    query.clear();
    custom = true;
    selected = nullptr;
    unit = "px";
    resolution = "300";
    new_width.set("1800");
    new_height.set("1200");
    notify(CanvasSetupChange::gallery);
}
void CanvasSetupModel::set_new_width(std::string value)
{
    const auto change = custom ? CanvasSetupChange::dimensions : CanvasSetupChange::gallery;
    custom = true;
    new_width.set(std::move(value));
    notify(change);
}
void CanvasSetupModel::set_new_height(std::string value)
{
    const auto change = custom ? CanvasSetupChange::dimensions : CanvasSetupChange::gallery;
    custom = true;
    new_height.set(std::move(value));
    notify(change);
}
void CanvasSetupModel::set_resolution(std::string value)
{
    const auto change = custom ? CanvasSetupChange::dimensions : CanvasSetupChange::gallery;
    custom = true;
    resolution = std::move(value);
    notify(change);
}
void CanvasSetupModel::change_unit(std::string value)
{
    if (value != "px" && value != "mm" && value != "in")
        return;
    const auto ppi = parsed(resolution);
    auto w = catalogue().convert(parsed(new_width.get()), unit, value, ppi);
    auto h = catalogue().convert(parsed(new_height.get()), unit, value, ppi);
    const auto rounding = value == "px" ? 1. : 1000.;
    const auto change = custom ? CanvasSetupChange::dimensions : CanvasSetupChange::gallery;
    custom = true;
    unit = std::move(value);
    new_width.set(decimal(std::floor(w * rounding + .5) / rounding));
    new_height.set(decimal(std::floor(h * rounding + .5) / rounding));
    notify(change);
}
void CanvasSetupModel::set_background(std::string value)
{
    background = std::move(value);
    notify(CanvasSetupChange::background);
}
void CanvasSetupModel::quick_size(int width, int height)
{
    custom = true;
    unit = "px";
    new_width.set(std::to_string(width));
    new_height.set(std::to_string(height));
    notify(CanvasSetupChange::dimensions);
}
void CanvasSetupModel::creation_error(std::string value)
{
    status.set(std::move(value));
    if (changed)
        changed(CanvasSetupChange::error);
}
void CanvasSetupModel::notify(CanvasSetupChange change)
{
    const auto spec = specification();
    can_create.set(bool(spec));
    status.set(spec ? "" : spec.error());
    if (changed)
        changed(change);
}
std::expected<CanvasDefinition, std::string> CanvasSetupModel::specification() const
{
    if (!custom && !selected)
        return std::unexpected("");
    auto spec = catalogue().specification(parsed(new_width.get()), parsed(new_height.get()), unit,
                                          parsed(resolution), background);
    if (spec)
    {
        spec->name = selection_name();
        spec->preset_id = custom ? "" : selected->id;
        spec->category = custom ? "Custom size" : selected->category;
        spec->section = custom ? "Your dimensions" : selected->section;
    }
    return spec;
}
std::string CanvasSetupModel::selection_name() const
{
    return custom ? "Custom canvas" : selected ? selected->name : "Choose a canvas";
}
std::string CanvasSetupModel::dimension_text() const
{
    return new_width.get() + " × " + new_height.get() + " " + unit;
}
std::string CanvasSetupModel::aspect_ratio() const
{
    auto spec = specification();
    if (!spec)
        return "—";
    const auto d = std::gcd(spec->pixel_width, spec->pixel_height);
    return std::to_string(spec->pixel_width / d) + ":" + std::to_string(spec->pixel_height / d);
}
std::vector<CanvasSection> CanvasSetupModel::sections() const
{
    return catalogue().sections(category_index, query);
}
std::size_t CanvasSetupModel::result_count() const
{
    std::size_t count = 0;
    for (const auto& group : sections())
        count += group.presets.size();
    return count;
}
} // namespace congregation::native
