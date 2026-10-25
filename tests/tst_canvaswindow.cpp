#include <Native/CanvasSetupWindow.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <iiFileProviderNative.h>
#include <iiImageCodec/ImageCodec.h>
#include <iiSDKCore/Runtime/Application.h>
#include <iostream>
#include <stdexcept>
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
void check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}
ii::gui::Widget find(ii::gui::WidgetMount mount, std::string_view id)
{
    if (auto direct = mount.find(id))
        return *direct;
    for (const auto& node : take(mount.snapshot()))
        if (node.id.ends_with("/" + std::string(id)))
            return take(mount.find(node.id));
    throw std::runtime_error("Missing chooser widget: " + std::string(id));
}
void capture(ii::gui::Host& host, const std::filesystem::path& path)
{
    const auto frame = take(host.capture_canvas());
    auto png = ii::image::encode_png(
        {frame.width, frame.height, ii::image::PixelFormat::rgba8, 0, frame.rgba});
    check(bool(png), "Cannot encode native chooser capture");
    iiFileProvider::native::File::write(path,
                                        {reinterpret_cast<const char*>(png->data()), png->size()});
}
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "Expected output directory and renderer");
        const bool vulkan = std::string_view(argv[2]) == "vulkan";
        const auto renderer = vulkan ? ii::gui::Renderer::vulkan : ii::gui::Renderer::automatic;
        const auto output = std::filesystem::path(argv[1]) / argv[2];
        std::filesystem::create_directories(output);
        ii::runtime::Application app({"canvas-window-contract"});
        auto parent_options = CanvasSetupWindow::options();
        parent_options.event_loop = &app.event_loop();
        parent_options.renderer = renderer;
        ii::gui::Host parent(parent_options);
        auto options = CanvasSetupWindow::options();
        options.event_loop = &app.event_loop();
        options.renderer = renderer;
        ii::gui::Host child(options);
        CanvasSetupWindow chooser;
        CanvasSetupModel* model = nullptr;
        int parent_actions = 0, creates = 0, closes = 0;
        ii::gui::WidgetMount parent_mount;
        std::exception_ptr failure;
        bool exercised = false, parent_survived = false;
        std::string stage = "open";
        const auto exercise = [&]
        {
            try
            {
                stage = "attach";
                good(chooser.attach(child,
                                    [&](CanvasSetupWindow::Scope& scope)
                                    {
                                        model = &scope.backend_0;
                                        model->create_requested = [&] { ++creates; };
                                        model->close_requested = [&]
                                        {
                                            ++closes;
                                            good(child.request_close());
                                        };
                                    }));
#if defined(__APPLE__)
                constexpr auto native_api = ii::gui::GraphicsApi::metal;
#elif defined(_WIN32)
                constexpr auto native_api = ii::gui::GraphicsApi::direct3d11;
#else
                constexpr auto native_api = ii::gui::GraphicsApi::vulkan;
#endif
                check(take(child.graphics_info()).api ==
                          (vulkan ? ii::gui::GraphicsApi::vulkan : native_api),
                      "Wrong chooser renderer");
                stage = "geometry";
                auto mount = chooser.widgets();
                const auto bounds = [&](std::string_view id)
                { return take(find(mount, id).snapshot()).bounds; };
                const auto edit = [&](std::string_view id, std::string value)
                {
                    good(find(mount, id).focus());
                    good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::key_down,
                                               .key = ii::gui::WidgetKey::select_all}));
                    good(mount.dispatch_input(
                        {.kind = ii::gui::WidgetInputKind::text, .text = std::move(value)}));
                    check(take(find(mount, id).snapshot()).focused,
                          "Editing rebuilt a focused input field");
                };
                check(bounds("setup-root").width == 1280 && bounds("setup-root").height == 880,
                      "Chooser client size changed");
                check(bounds("canvasHeader").x == 12 && bounds("canvasHeader").height == 96,
                      "Header differs from Dreamscapes");
                check(bounds("canvasCategoryScroll").width == 200 &&
                          bounds("canvasDetailsScroll").width == 268,
                      "Desktop columns changed");
                const auto first = bounds("canvasPresetCard_01-01-01"),
                           third = bounds("canvasPresetCard_01-01-03");
                check(first.height == 144 && first.y == third.y && third.x > first.x,
                      "Gallery is not a three-column card grid");
                check(bounds("canvasPreset_01-01-01").height == 44,
                      "Preset metadata is not a static 44px row");
                for (const auto& node : take(mount.snapshot()))
                    check(!node.modal && !node.dim, "Child window introduced a modal surface");
                good(take(parent_mount.find("parent-action")).activate());
                check(parent_actions == 1, "Parent input was blocked");
                stage = "capture";
                const auto pixels = take(child.capture_canvas());
                const auto scale = pixels.width / 1280.;
                const auto pixel = [&](double x, double y, std::array<int, 3> expected)
                {
                    auto index = (std::size_t((y + .5) * scale) * pixels.width +
                                  std::size_t((x + .5) * scale)) *
                                 4;
                    for (int c = 0; c < 3; ++c)
                        check(std::abs(int(pixels.rgba[index + c]) - expected[c]) <= 2,
                              "Chooser authored surface color changed");
                };
                pixel(16, 16, {36, 36, 36});
                pixel(16, 112, {32, 32, 32});
                pixel(0, 112, {90, 93, 99});
                capture(child, output / "desktop.png");
                stage = "card selection";
                good(find(mount, "canvasPresetCard_01-01-02").activate());
                check(model->new_height.get() == "1350",
                      "Card activation did not select its dimensions");
                stage = "find shortcut";
                good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::key_down,
                                           .key = ii::gui::WidgetKey::character,
                                           .text = "f",
                                           .meta = true}));
                check(take(find(mount, "canvasPresetSearch").snapshot()).focused,
                      "Find did not focus search");
                stage = "print search";
                model->search("A4");
                check(model->result_count() == 11 && model->unit == "mm",
                      "Global print search failed");
                check(take(find(mount, "canvasPpiInput").snapshot()).visible,
                      "Physical units did not expose PPI");
                capture(child, output / "print.png");
                good(find(mount, "canvasSearchClear").activate());
                check(model->query.empty() && model->result_count() == 58,
                      "Search clear affordance did not reset results");
                edit("canvasPresetSearch", "A4");
                check(model->query == "A4" && model->result_count() == 11,
                      "Native search typing did not reach the catalogue");
                capture(child, output / "print.png");
                stage = "units and background menus";
                good(find(mount, "canvasUnitsButton").activate());
                check(take(find(mount, "canvasUnitsMenu").snapshot()).opened,
                      "Units menu did not open");
                good(find(mount, "canvasUnit-in").activate());
                check(model->unit == "in" && model->custom,
                      "Unit menu did not convert the physical canvas");
                good(find(mount, "canvasBackgroundButton").activate());
                good(find(mount, "canvasBackground-Transparent").activate());
                check(model->background == "Transparent", "Background menu selection was ignored");
                edit("canvas-width", "0");
                check(!model->can_create.get(), "Edited invalid width was accepted");
                capture(child, output / "invalid.png");
                edit("canvas-width", "8.268");
                check(model->can_create.get(), "Native text edit did not update the specification");
                edit("canvasPpiInput", "150");
                check(model->specification()->pixel_width == 1240 && model->resolution == "150",
                      "PPI input did not update output pixels");
                stage = "empty results";
                model->search("no-such-format");
                check(!take(find(mount, "create-canvas").snapshot()).enabled,
                      "Empty results kept Create enabled");
                good(find(mount, "canvasClearSearchButton").activate());
                check(model->selected && model->can_create.get(), "Empty-result recovery failed");
                stage = "custom";
                good(find(mount, "canvasCustomButton").activate());
                model->set_background("White");
                check(model->custom && model->new_width.get() == "1800",
                      "Custom action defaults differ");
                good(find(mount, "canvasQuick-1920-1080").activate());
                check(model->aspect_ratio() == "16:9", "Quick aspect selection failed");
                model->set_new_width("0");
                good(find(mount, "create-canvas").activate());
                check(creates == 0, "Invalid input created a document");
                model->set_new_width("1920");
                good(find(mount, "create-canvas").activate());
                check(creates == 1, "Valid Create action was not delivered");
                capture(child, output / "custom.png");
                good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::key_down,
                                           .key = ii::gui::WidgetKey::enter,
                                           .control = true}));
                check(creates == 2, "Control Return did not deliver the Create action");
                for (auto size :
                     {ii::gui::Size{640, 480}, ii::gui::Size{390, 844}, ii::gui::Size{320, 640}})
                {
                    stage = "resize " + std::to_string(int(size.width));
                    good(child.set_content_size(size));
                    check(take(find(mount, "canvasCategoryDropdown").snapshot()).visible,
                          "Compact category menu missing");
                    auto scroll = take(find(mount, "canvasCompactScroll").snapshot());
                    check(scroll.content_height > scroll.bounds.height,
                          "Compact chooser content was clipped instead of scrolling");
                    check(bounds("canvasFooter").y + bounds("canvasFooter").height <= size.height,
                          "Footer escaped compact window");
                    capture(child, output / (std::to_string(int(size.width)) + ".png"));
                    good(find(mount, "canvasCompactScroll")
                             .set_scroll_offset(scroll.content_height));
                    const auto detail = bounds("canvasBackgroundButton"),
                               viewport = bounds("canvasCompactScroll");
                    check(detail.y >= viewport.y &&
                              detail.y + detail.height <= viewport.y + viewport.height,
                          "Compact scrolling did not reveal the detail controls");
                    capture(child, output / (std::to_string(int(size.width)) + "-details.png"));
                    good(find(mount, "canvasCompactScroll").set_scroll_offset(0));
                }
                stage = "restore desktop";
                good(child.set_content_size({1280, 880}));
                model->choose_category(0);
                const auto last = model->catalogue().sections(0).back().presets.back();
                const auto id = "canvasPresetCard_" + model->catalogue().presets()[last].id;
                stage = "reveal card";
                good(find(mount, id).focus());
                good(find(mount, id).activate());
                const auto focused = bounds(id), gallery = bounds("canvasGalleryScroll");
                capture(child, output / "focused-card.png");
                check(focused.y + focused.height <= gallery.y + gallery.height + 1,
                      "Keyboard selection was not revealed in the gallery");
                stage = "escape";
                good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::key_down,
                                           .key = ii::gui::WidgetKey::escape}));
                check(closes == 1, "Escape did not close only the child window");
                exercised = true;
            }
            catch (...)
            {
                failure = std::current_exception();
                std::cerr << "Canvas assertion failed at " << stage << '\n';
                if (auto state = child.snapshot(); state && !state->closed)
                    (void)child.request_close();
            }
        };
        auto run = parent.run(
            {.opened = [&]
             {
                 parent_mount = take(parent.mount_widgets(
                     [&](ii::gui::WidgetBuilder& b) -> ii::core::Result<void>
                     {
                         auto button = b.button({}, "parent-action", {.text = "Parent document"},
                                                [&] { ++parent_actions; });
                         if (!button)
                             return std::unexpected(button.error());
                         return {};
                     }));
                 good(child.show(parent, {.opened =
                                              [&]
                                          {
                                              check(bool(child.executor().post(exercise)),
                                                    "Cannot schedule child-window assertions");
                                          },
                                          .closed =
                                              [&]
                                          {
                                              auto state = parent.snapshot();
                                              parent_survived =
                                                  state && state->visible && !state->closed;
                                              if (state && !state->closed)
                                                  good(parent.request_close());
                                          }}));
             }});
        if (failure)
        {
            std::cerr << "Canvas window stage: " << stage << '\n';
            std::rethrow_exception(failure);
        }
        good(std::move(run));
        check(exercised, "Native child callback did not execute the assertions");
        check(parent_survived, "Closing the child also closed the parent document window");
        std::cout << "CANVAS_WINDOW_OK subwindow=true modal=false presets=289 responsive=true\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
