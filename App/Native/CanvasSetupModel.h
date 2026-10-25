#pragma once
#include "CanvasDefinition.h"
#include <expected>
#include <functional>
#include <iiSDKCore/Core/Property/Observable.h>
#include <optional>
#include <string>
#include <vector>

namespace congregation::native
{
struct CanvasPreset final
{
    std::string id, name, category, section, unit;
    double width = 0, height = 0, ppi = 300;
    int pixel_width = 0, pixel_height = 0;
    std::size_t category_index = 0;
};
struct CanvasCategory final
{
    std::string name, subtitle;
    std::size_t count = 0;
};
struct CanvasSection final
{
    std::string name;
    std::vector<std::size_t> presets;
};
class CanvasCatalogue final
{
  public:
    explicit CanvasCatalogue(std::string_view json);
    static const CanvasCatalogue& bundled();
    const std::vector<CanvasPreset>& presets() const { return presets_; }
    const std::vector<CanvasCategory>& categories() const { return categories_; }
    const CanvasPreset* find(std::string_view) const;
    std::vector<CanvasSection> sections(std::size_t category, std::string_view query = {}) const;
    static std::expected<CanvasDefinition, std::string>
    specification(double width, double height, std::string unit, double ppi = 300,
                  std::string background = "White");
    static double convert(double value, std::string_view from, std::string_view to, double ppi);

  private:
    std::vector<CanvasPreset> presets_;
    std::vector<CanvasCategory> categories_;
};
enum class CanvasSetupChange
{
    gallery,
    selection,
    dimensions,
    background,
    error
};
class CanvasSetupModel final
{
  public:
    CanvasSetupModel();
    void begin();
    void choose_category(std::size_t);
    bool choose(std::string_view id);
    void search(std::string);
    void custom_size();
    void set_new_width(std::string);
    void set_new_height(std::string);
    void set_resolution(std::string);
    void change_unit(std::string);
    void set_background(std::string);
    void quick_size(int width, int height);
    void creation_error(std::string);
    std::expected<CanvasDefinition, std::string> specification() const;
    std::string selection_name() const;
    std::string dimension_text() const;
    std::string aspect_ratio() const;
    std::vector<CanvasSection> sections() const;
    std::size_t result_count() const;
    const CanvasCatalogue& catalogue() const { return CanvasCatalogue::bundled(); }
    std::size_t category_index = 0;
    std::string query, unit = "px", resolution = "300", background = "White";
    bool custom = false;
    const CanvasPreset* selected = nullptr;
    ii::core::Observable<std::string> new_width{"1080"}, new_height{"1080"}, status{""};
    ii::core::Observable<bool> can_create{true};
    std::function<void(CanvasSetupChange)> changed;
    std::function<void()> create_requested, close_requested;

  private:
    void notify(CanvasSetupChange);
    void select(const CanvasPreset&);
};
} // namespace congregation::native
