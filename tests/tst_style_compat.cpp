#include <LVRS/Theme.h>
#include <LVRS/Icons.h>
#include <FigmaColors.h>
#include "CongregationStyle.generated.h"
#include <cmath>
#include <array>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-12; }
bool same(ii::gui::WidgetColor a, ii::gui::WidgetColor b) {
    return near(a.red,b.red) && near(a.green,b.green) && near(a.blue,b.blue) && near(a.alpha,b.alpha);
}
}
int main() {
    constexpr std::array<lvrs::Accent,10> accents{lvrs::Accent::blue,lvrs::Accent::red,
        lvrs::Accent::green,lvrs::Accent::yellow,lvrs::Accent::purple,lvrs::Accent::rose,
        lvrs::Accent::orange,lvrs::Accent::brown,lvrs::Accent::cyan,lvrs::Accent::teal};
    for (std::size_t i=0;i<accents.size();++i)
        check(same(lvrs::Theme::accent_color(accents[i]),lvrs::figma_colors::accents[i][0]),
              "A native accent did not match its Figma token");
    const auto blue = lvrs::figma_colors::accents[0][0];
    const auto hover = lvrs::figma_colors::accents[0][1];
    const auto pressed = lvrs::figma_colors::accents[0][2];
    const auto appearance = lvrs::Theme(lvrs::Accent::blue).appearance();
    check(same(lvrs::Theme::accent_color(lvrs::Accent::blue), blue), "Native accent is stale");
    check(same(appearance.primary,blue) && same(appearance.hover,hover) && same(appearance.pressed,pressed),
          "Native accent states are stale");
    const auto base = lvrs::figma_colors::backgrounds[19];
    const auto mix = [](double x,double y) { return std::round((x*.98+y*.02)*255)/255; };
    check(near(appearance.window.red,mix(base.red,blue.red)) &&
          near(appearance.window.green,mix(base.green,blue.green)) &&
          near(appearance.window.blue,mix(base.blue,blue.blue)), "Native window token is stale");
    using Color = congregation::native::style::Color;
    check(same(Color::blue, blue) && same(Color::blueHover, hover) && same(Color::bluePressed, pressed),
          "AOT compiler did not use current Figma colors");
    check(same(Color::window, base), "Application window must use the shared LVRS window token");
    for (auto [name, expected] : {std::pair<std::string_view,std::string_view>{"generaladd","#2373CD"},
                                  {"nodesfolder","#CDAE30"}}) {
        const auto icon = lvrs::Icons::data(name);
        check(icon && icon->find(expected) != std::string_view::npos, "Native icon did not use the darker token");
    }
    for (const auto name : {"generalopen", "generalsave", "generalsearch", "rectangle", "imagezoomOut", "imagezoomIn"}) {
        const auto image = lvrs::Icons::image(name, 18);
        check(image && *image && (*image)->width() == 18 && (*image)->height() == 18,
              "A native control icon must resolve and rasterize from LVRS");
    }
}
