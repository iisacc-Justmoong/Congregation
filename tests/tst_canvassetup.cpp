#include <Native/CanvasSetupModel.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
using namespace congregation::native;
namespace
{
void check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}
} // namespace
int main()
{
    try
    {
        const auto& c = CanvasCatalogue::bundled();
        check(c.categories().size() == 14 && c.presets().size() == 289,
              "Dreamscapes catalogue incomplete");
        check(c.categories()[0].count == 58 && c.sections(0).size() == 13,
              "Social platform groups changed");
        std::set<std::string> ids;
        for (const auto& p : c.presets())
        {
            check(ids.insert(p.id).second && c.find(p.id) == &p, "Preset identity changed");
            auto s = c.specification(p.width, p.height, p.unit, p.ppi);
            check(s && s->pixel_width == p.pixel_width && s->pixel_height == p.pixel_height,
                  "Authored preset rounding changed");
        }
        std::size_t count = 0;
        for (const auto& section : c.sections(0, "A4"))
            count += section.presets.size();
        check(count == 11, "Search must span all categories");
        check(!c.sections(0, "1920 × 1080").empty() && !c.sections(0, "1920x1080").empty(),
              "Size search normalization changed");
        check(c.sections(0, "X").size() == 1 && c.sections(0, "X")[0].name == "Social media · X",
              "X search matched ordinary letters");
        check(c.sections(0, "TikTok").empty() && c.sections(0, "KakaoTalk").empty() &&
                  c.sections(0, "LINE rich menu").empty(),
              "Removed formats reappeared");
        auto a4 = c.specification(210, 297, "mm", 300);
        check(a4 && a4->pixel_width == 2480 && a4->pixel_height == 3508,
              "A4 physical conversion changed");
        auto tie = c.specification(2.5, 3.5, "in", 1);
        check(tie && tie->pixel_width == 2 && tie->pixel_height == 4,
              "Nearest-even rounding changed");
        check(c.convert(25.4, "mm", "in", 300) == 1, "Unit conversion reinterpreted the value");
        for (auto s : {c.specification(0, 1, "px"), c.specification(.5, 1, "px"),
                       c.specification(32769, 1, "px"), c.specification(32768, 32768, "px"),
                       c.specification(100, 100, "cm"), c.specification(100, 100, "px", 2401),
                       c.specification(100, 100, "px", 300, "Invalid"),
                       c.specification(std::numeric_limits<double>::quiet_NaN(), 1, "px")})
            check(!s, "Invalid canvas escaped validation");
        CanvasSetupModel model;
        check(model.selected && model.selected->id == "01-01-01" && model.background == "White",
              "Initial selection differs from Dreamscapes");
        model.search("A4");
        check(model.result_count() == 11 && model.selected,
              "Search did not select its first result");
        model.search("not-a-format");
        check(!model.can_create.get() && !model.selected, "Empty results allowed creation");
        model.custom_size();
        check(model.custom && model.new_width.get() == "1800" && model.can_create.get(),
              "Custom size defaults changed");
        model.set_new_width("0");
        check(!model.can_create.get(), "Invalid draft allowed creation");
        model.quick_size(1920, 1080);
        check(model.aspect_ratio() == "16:9", "Quick aspect ratio changed");
        model.change_unit("in");
        check(model.custom && model.new_width.get() == "6.4",
              "Unit change lost the existing physical extent");
        model.change_unit("px");
        check(model.specification()->pixel_width == 1920, "Unit round trip changed pixels");
        model.set_background("Black");
        check(model.specification()->background == "Black", "Background selection ignored");
        model.begin();
        check(model.background == "White" && model.selected->id == "01-01-01",
              "Reopen retained stale selection");
        std::cout << "CANVAS_SETUP_MODEL_OK presets=289 categories=14 search=true conversion=true "
                     "validation=true\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
