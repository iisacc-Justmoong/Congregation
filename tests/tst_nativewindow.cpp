#include <Native/MainWindow.h>
#include <iiSDKCore/Runtime/Application.h>
#include <iostream>
#include <stdexcept>
#include <array>
#include <cmath>
using namespace congregation::native;
namespace
{
template <class T> T take(ii::core::Result<T> r)
{
    if (!r)
    {
        if (r.error().cause)
            std::rethrow_exception(r.error().cause);
        throw std::runtime_error(r.error().message);
    }
    return std::move(*r);
}
void good(ii::core::Result<void> r)
{
    if (!r)
    {
        if (r.error().cause)
            std::rethrow_exception(r.error().cause);
        throw std::runtime_error(r.error().message);
    }
}
void require(bool b, const char* m)
{
    if (!b)
        throw std::runtime_error(m);
}
} // namespace
int main(int argc, char** argv)
{
    try
    {
        require(argc == 2 || (argc == 3 && std::string_view(argv[2]) == "vulkan"),
                "Expected asset directory and optional vulkan renderer");
        ii::runtime::Application app({"window-contract"});
        auto options = MainWindow::options();
        if (argc == 3)
            options.renderer = ii::gui::Renderer::vulkan;
        options.event_loop = &app.event_loop();
        options.frame_loop = &app.frame_loop();
        ii::gui::Host host(options);
        MainWindow view;
        view.set_assets(argv[1]);
        congregation::ui::EditorBackend* vm = nullptr;
        good(host.run(
            {.opened = [&]
             {
                 good(host.set_content_size({1999, 1080}));
                 good(view.attach(host,
                                  [&](MainWindow::Scope& scope)
                                  {
                                      vm = &scope.backend_0;
                                      vm->licensed.set(true);
                                      vm->needs_license.set(false);
                                  }));
                 auto mount = view.widgets();
                 const auto root = take(take(mount.find("root")).snapshot());
                 require(root.bounds.width == 1999 && root.bounds.height == 1080,
                         "ApplicationWindow size changed");
                 const auto checkWorkspace = [&]
                 {
                     require(!mount.find("sidebar") && !mount.find("asset-results"),
                             "The removed asset sidebar is still mounted");
                     const auto rail = take(take(mount.find("tool-rail")).snapshot()).bounds;
                     const auto center =
                         take(take(mount.find("canvas-and-timeline")).snapshot()).bounds;
                     const auto inspector = take(take(mount.find("inspector")).snapshot()).bounds;
                     const auto timeline = take(take(mount.find("timeline")).snapshot()).bounds;
                     const auto window = take(take(mount.find("root")).snapshot()).bounds;
                     require(rail.x == 0 && rail.width == 54,
                             "The vertical toolbar must sit at the left window edge");
                     require(inspector.width == 352 &&
                                 inspector.x + inspector.width == window.width,
                             "The right inspector must retain its width and position");
                     require(center.x == rail.width && center.x + center.width == inspector.x &&
                                 timeline.x == center.x && timeline.width == center.width,
                             "Canvas and timeline did not reclaim the removed sidebar width");
                 };
                 checkWorkspace();
                 good(take(mount.find("eraser")).activate());
                 require(vm->selected_tool.get() == "eraser",
                         "The retained vertical toolbar no longer selects tools");
                 good(take(mount.find("brush")).activate());
                 require(vm->selected_tool.get() == "brush",
                         "The retained toolbar cannot return to the brush");
                 const auto pixels = take(host.capture_canvas());
                 const auto pixelScale = pixels.width / 1999.;
                 const auto colorAt = [&](double x, double y)
                 {
                     const auto index = (std::size_t((y + .5) * pixelScale) * pixels.width +
                                         std::size_t((x + .5) * pixelScale)) *
                                        4;
                     return std::array<int, 3>{pixels.rgba[index], pixels.rgba[index + 1],
                                               pixels.rgba[index + 2]};
                 };
                 const auto checkColor = [&](double x, double y, std::array<int, 3> expected)
                 {
                     const auto actual = colorAt(x, y);
                     for (int channel = 0; channel < 3; ++channel)
                         if (std::abs(actual[channel] - expected[channel]) > 2)
                             throw std::runtime_error(
                                 "Figma color mismatch at " + std::to_string(x) + "," +
                                 std::to_string(y) + ": actual=" + std::to_string(actual[0]) + "," +
                                 std::to_string(actual[1]) + "," + std::to_string(actual[2]));
                 };
                 checkColor(400, 80, {20, 23, 25});
                 for (const auto& position : std::array<std::array<double, 2>, 4>{
                          {{1000, 27}, {53, 700}, {1400, 764}, {1800, 380}}})
                     checkColor(position[0], position[1], {12, 12, 12});
                 checkColor(1646, 700, {12, 12, 12});
                 checkColor(1647, 700, {20, 23, 25});
                 const auto divider = take(take(mount.find("color-divider")).snapshot()).bounds;
                 checkColor(divider.x + 100, divider.y, {35, 36, 36});
                 const auto ruler = take(take(mount.find("timeline-ruler")).snapshot()).bounds;
                 const auto playhead = ruler.x + 416 + (ruler.width - 416) / 2;
                 checkColor(playhead, ruler.y + 24, {84, 138, 247});
                 const auto layers = take(take(mount.find("layers")).snapshot()).bounds;
                 checkColor(playhead, layers.y + 50, {84, 138, 247});
                 for (const auto& p : ToolSettings::specs())
                 {
                     auto slider = take(mount.find(p.id == ToolParameter::size
                                                       ? "brush-size"
                                                       : "adjust-" + std::string(p.key)));
                     require(take(slider.snapshot()).bounds.height == 32,
                             "Parameter slider hit height changed");
                     require(
                         take(take(mount.find("value-" + std::string(p.key))).snapshot()).text ==
                             vm->parameters->label(p.id),
                         "Displayed parameter differs from the model");
                 }
                 good(take(mount.find("size-plus")).activate());
                 require(vm->parameters->value(ToolParameter::size) == 65,
                         "Step button does not edit shared Tool state");
                 auto stale = take(mount.find("brush-size"));
                 auto settings = vm->parameters;
                 good(take(mount.find("option-1")).activate());
                 require(take(take(mount.find("document-detail")).snapshot()).visible,
                         "Document segment did not switch panels");
                 good(take(mount.find("option-0")).activate());
                 auto scroll = take(mount.find("tool-scroll"));
                 good(scroll.set_scroll_offset(5000));
                 require(take(scroll.snapshot()).scroll_offset > 0,
                         "Tool parameters are not scrollable");
                 auto rail = take(take(mount.find("color-transparency")).snapshot()).bounds;
                 good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::pointer_down,
                                            .x = rail.x + 11,
                                            .y = rail.y + rail.height / 2,
                                            .buttons = 1}));
                 good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::pointer_up,
                                            .x = rail.x + 11,
                                            .y = rail.y + rail.height / 2}));
                 require(take(vm->color->state()).color.alpha == .5,
                         "The transparency rail does not edit the shared color model");
                 good(vm->color->edit_channel(ii::gui::ColorChannel::alpha, 100));
                 good(host.set_content_size({1200, 720}));
                 const auto small = take(take(mount.find("canvas")).snapshot());
                 require(small.bounds.width > 0 && small.bounds.height > 0,
                         "The minimum window size lost its canvas");
                 checkWorkspace();
                 good(host.set_content_size({1999, 1080}));
                 good(view.detach(host));
                 require(!stale.snapshot(), "Retired window handles remained usable");
                 settings->set(ToolParameter::size, 66);
                 good(host.request_close());
             }}));
        std::cout << "NATIVE_WINDOW_CONTRACT_OK sidebar=false tool_rail=true parameters=22 geometry=true input=true scroll=true "
                     "generation=true figma_colors=true qt=false\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
