#include "Artwork.h"
#include "CanvasView.h"
#include "EditorBackend.h"
#include "EditorLayout.h"
#include "EditorIcons.h"
#include "MainView.generated.h"
#include "PreferencesView.generated.h"
#include "CanvasSetupView.generated.h"
#include <Engine/ApplicationEngine.h>
#include <Engine/Preferences.h>
#include <Engine/LicenseService.h>
#include <Engine/CanvasSession.h>
#include <Engine/NearbyDiscovery.h>
#include <iiGUI/Application.h>
#include <iiGUI/Desktop.h>
#include <iiImageCodec/ImageCodec.h>
#include <iiUpdateManager/NativeUpdateManager.h>
#include <iiLicenseManager/NativeLicenseClient.h>
#include <iiFileProviderNative.h>
#include <iiFileProviderCompatibility.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <charconv>
#include <iostream>
#include <optional>
#include <utility>

namespace congregation::native
{
using namespace iiSharedCanvas;
using MainView = lvui_generated::MainView;
using PreferencesView = lvui_generated::PreferencesView;
using CanvasSetupView = lvui_generated::CanvasSetupView;
using ui::Action;
namespace
{
void check(bool value, std::string message)
{
    if (!value)
        throw std::runtime_error(std::move(message));
}
template <class T> T take(ii::core::Result<T> result)
{
    if (!result)
    {
        if (result.error().cause)
            std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
    return std::move(*result);
}
template <class T> T take(ii::image::Result<T> result)
{
    if(!result)throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(ii::core::Result<void> result)
{
    if (!result)
    {
        if (result.error().cause)
            std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
}
CredentialPort credentials(const std::shared_ptr<iisacc::accounts::native::SecureStore>& store,
                           bool test)
{
    if (test)
        return {.supported = [] { return false; },
                .read = [](auto done) { done(CredentialPort::ReadStatus::not_found, {}); },
                .write = [](auto, auto done) { done(false); },
                .remove = [](auto done) { done(false); }};
    return {.supported = [store] { return store->supported(); },
            .read =
                [store](auto done)
            {
                auto value = store->read();
                done(value.status, std::move(value.bytes));
            },
            .write = [store](auto bytes, auto done) { done(store->write(bytes)); },
            .remove = [store](auto done) { done(store->remove()); }};
}
class UpdateCredentials final : public iisacc::updates::native::UpdateCredentialProvider
{
  public:
    explicit UpdateCredentials(LicenseService& value) : license_(value) {}
    void requestCredentials(Completion completion) override
    {
        auto generation = generation_;
        const auto expected = *generation;
        license_.credentials(
            [generation, expected, completion = std::move(completion)](auto status,
                                                                       Credentials value) mutable
            {
                if (*generation != expected)
                    return;
                if (status != LicenseService::StoredStatus::available)
                {
                    completion(std::nullopt, "A saved Congregation license is required");
                    return;
                }
                iisacc::updates::native::UpdateCredentials out;
                out.email = std::move(value.email);
                out.licenseKey = std::move(value.key);
                completion(std::move(out), {});
            });
    }
    void cancel() noexcept override { ++*generation_; }
    ~UpdateCredentials() override { cancel(); }

  private:
    LicenseService& license_;
    std::shared_ptr<unsigned> generation_ = std::make_shared<unsigned>(0);
};
} // namespace
struct State final : std::enable_shared_from_this<State>
{
    ii::runtime::Application& app;
    bool test = false, closed = false, receiving = false;
    int pressure_handle = -1;
    ii::gui::Renderer renderer = ii::gui::Renderer::automatic;
    std::filesystem::path directory, initial_file, reference_image;
    ApplicationEngine engine;
    Preferences preferences;
    Artwork artwork;
    CanvasView canvas;
    std::shared_ptr<ii::gui::Host> host, preferences_host, setup_host;
    std::shared_ptr<CanvasSetupView> setup_view;
    ui::EditorBackend* setup = nullptr;
    std::shared_ptr<const ii::gui::RasterImage> avatar;
    std::shared_ptr<MainView> view = std::make_shared<MainView>();
    std::shared_ptr<PreferencesView> preferences_view;
    std::shared_ptr<lvrs::ListModel> layers = std::make_shared<lvrs::ListModel>(),
                                     members = std::make_shared<lvrs::ListModel>();
    std::shared_ptr<ii::gui::ColorPickerModel> color =
        std::make_shared<ii::gui::ColorPickerModel>();
    std::shared_ptr<const ii::gui::ColorChangePort> color_port;
    ui::EditorBackend* main = nullptr;
    ui::EditorBackend* secondary = nullptr;
    std::shared_ptr<iisacc::accounts::native::SecureStore> store =
        std::make_shared<iisacc::accounts::native::SecureStore>("com.iisacc.congregation.painter",
                                                                "account-license");
    LicenseService license;
    UpdateCredentials update_credentials;
    iisacc::updates::native::UpdateManager updater;
    ii::network::Http image_http;
    std::string selected_member;
    iisacc::licensing::native::LicenseClient account;
    NearbyDiscovery discovery;
    CanvasSession session;
    ii::runtime::TaskToken deadline, notice_deadline;
    std::function<void()> after_setup, after_preferences;
    State(ii::runtime::Application& application, bool self_test, std::filesystem::path data)
        : app(application), test(self_test), directory(std::move(data)), engine(app.event_loop()),
          preferences(test ? directory / "preferences.ini" : std::filesystem::path{},
                      directory / "recovery"),
          artwork(directory / "recovery"), canvas(artwork),
          license(app.event_loop(), "https://iisacc.com/graphql", 10000, !test,
                  credentials(store, test)),
          update_credentials(license),
          updater(app.event_loop(), "congregation", CONGREGATION_VERSION),
          image_http(app.event_loop()),
          account(app.event_loop(), "congregation", "https://iisacc.com/graphql",
                  directory / "licenses"),
          discovery(app.event_loop(), {}), session(app.event_loop(), discovery.id)
    {
        check(artwork.create(test ? 128 : 1024, test ? 96 : 768), artwork.error());
    }
    void status(std::string value)
    {
        if(setup)setup->status.set(value);
        if (main)
            main->status.set(value);
        if (secondary)
            secondary->status.set(std::move(value));
    }
    void result(bool ok, std::string success = {})
    {
        if (!ok) {
            status(artwork.error().empty() ? "The operation could not be completed" : artwork.error());
            if(view&&host) {
                auto mount=view->widgets();auto anchor=mount.find("canvas");auto notice=mount.find("status-notice");
                if(anchor&&notice) {
                    good(notice->open_for(*anchor,16,std::max(0.f,take(anchor->snapshot()).bounds.height-108)));
                    (void)notice_deadline.cancel();
                    const auto weak=weak_from_this();
                    auto scheduled=host->executor().after(std::chrono::seconds(5),[weak]{if(auto p=weak.lock())if(auto widget=p->view->widgets().find("status-notice"))(void)widget->close_popup();});
                    if(scheduled)notice_deadline=scheduled.token;
                }
            }
        }
        else if (!success.empty())
            status(std::move(success));
    }
    void refresh()
    {
        std::vector<lvrs::ListRowData> rows;
        for (auto it = artwork.document().layers.rbegin(); it != artwork.document().layers.rend();
             ++it)
        {
            const auto& p = layerProperties(*it);
            rows.push_back({p.id, p.name, "", true, true, p.visible});
        }
        take(layers->set_rows(std::move(rows)));
        if (!artwork.selected().empty())
            take(layers->select(artwork.selected()));
        const auto title =
            artwork.source().empty() ? "Untitled" : path_text(artwork.source().filename());
        if (main)
            main->title.set(title);
        if (secondary)
            secondary->title.set(title);
        canvas.refresh();
        if (main)
            main->zoom.set(ii::core::format_decimal(std::round(canvas.viewport().scale * 100)) +
                           "%");
    }
    void changed()
    {
        refresh();
        if (!test)
            preferences.record_recovery(artwork.recovery());
        if (receiving)
            return;
        if (session.state == CanvasSession::State::hosting)
            session.publish(artwork.snapshot());
        else if (session.state == CanvasSession::State::connected)
        {
            auto bytes = artwork.snapshot();
            if (!session.submit({{"type", Value("nativeDocument")},
                                 {"document", Value(base64(bytes))},
                                 {"baseRevision", Value(session.revision)}}))
                status(session.error);
        }
    }
    void bind(ui::EditorBackend& backend, bool auxiliary)
    {
        if (auxiliary)
            secondary = &backend;
        else
            main = &backend;
        backend.layers = layers;
        backend.members = members;
        backend.color = color;
        backend.licensed.set(test || license.licensed);
        backend.needs_license.set(!test && !license.licensed);
        backend.start_recent.set(preferences.start_recent);
        backend.start_new.set(!preferences.start_recent);
        backend.can_invite.set(preferences.invitations_allowed);
        backend.discover.set(preferences.discover_nearby);
        backend.display_name.set(preferences.profile_name);
        backend.account_email.set(main && !main->email.get().empty() ? main->email.get() : "Not connected");
        if (auxiliary && main)
        {
            backend.spacing_pixels.set(main->spacing_pixels.get());
            backend.flow.set(main->flow.get());
            backend.hardness.set(main->hardness.get());
            backend.spacing.set(main->spacing.get());
            backend.stabilizer.set(main->stabilizer.get());
            backend.pressure_enabled.set(main->pressure_enabled.get());
            backend.pressure_min.set(main->pressure_min.get());
            backend.pressure_mid.set(main->pressure_mid.get());
            backend.pressure_max.set(main->pressure_max.get());
        }
        const auto weak = weak_from_this();
        backend.perform = [weak](Action action)
        {
            if (auto p = weak.lock())
                p->act(action);
        };
        backend.selected = [weak](std::string key)
        {
            if (auto p = weak.lock())
                p->result(p->artwork.select(std::move(key)));
        };
        backend.renamed = [weak](std::string key, std::string name)
        {
            if (auto p = weak.lock())
                p->result(p->artwork.rename(std::move(key), std::move(name)));
        };
        backend.moved = [weak](std::string key, std::string parent, std::string before)
        {
            if (auto p = weak.lock())
            {
                if (!parent.empty())
                {
                    p->status("Canvas layers form a flat compositing stack");
                    p->refresh();
                    return;
                }
                p->result(p->artwork.move(std::move(key), std::move(before)));
            }
        };
        backend.checked = [weak](std::string key, bool value)
        {
            if (auto p = weak.lock())
                p->result(p->artwork.visible(std::move(key), value));
        };
        backend.member_selected = [weak](std::string key)
        {
            if (auto p = weak.lock())
            {
                p->selected_member = key;
                if (key.starts_with("nearby-"))
                    for (const auto& item : p->discovery.canvases())
                    {
                        auto peer = object(item);
                        if (key == "nearby-" + text(field(peer, "sessionId")) && p->secondary)
                            p->secondary->session_address.set(
                                text(field(peer, "sessionId")) + "@" +
                                text(field(peer, "address")) + ":" +
                                std::to_string(int(number(field(peer, "port")))));
                    }
            }
        };
        backend.pressure_input=[weak](const ii::gui::WidgetInput& e)->ii::core::Result<bool>{
            auto p=weak.lock();if(!p||!p->main)return false;
            auto widget=take(p->view->widgets().find("pressure-curve"));
            using K=ii::gui::WidgetInputKind;
            const auto bounds=take(widget.snapshot()).bounds;
            const auto x=e.x-bounds.x,y=e.y-bounds.y;
            if(e.kind==K::pointer_down){p->pressure_handle=editor::pressure_point(x,bounds.width);good(widget.focus());}
            if(e.kind==K::focus_lost||e.kind==K::pointer_leave){p->pressure_handle=-1;return false;}
            if((e.kind==K::pointer_down||e.kind==K::pointer_move||e.kind==K::pointer_up)&&p->pressure_handle>=0){
                const auto value=editor::pressure_value(y);
                if(p->pressure_handle==0)p->main->set_pressure_min(value);
                else if(p->pressure_handle==1)p->main->set_pressure_mid(value);
                else p->main->set_pressure_max(value);
                if(e.kind==K::pointer_up)p->pressure_handle=-1;
                return true;
            }
            return false;
        };
        backend.input = [weak](const ii::gui::WidgetInput& event) -> ii::core::Result<bool>
        {
            if (auto p = weak.lock())
            {
                if (!p->test && !p->license.licensed)
                    return false;
                if (p->main)
                {
                    p->canvas.brush_size = p->main->brush_size.get();
                    p->canvas.opacity = p->main->opacity.get();
                    p->canvas.text = p->main->text_value.get();
                }
                auto local=event;
                const auto bounds=take(take(p->view->widgets().find("canvas")).snapshot()).bounds;
                local.x-=bounds.x;local.y-=bounds.y;
                if (event.kind == ii::gui::WidgetInputKind::drop && event.content &&
                    !event.content->url.empty() && event.content->image.empty() &&
                    event.content->files.empty())
                {
                    auto v = p->canvas.viewport();
                    p->fetch_image(event.content->url,
                                   {(local.x - v.x) / v.scale, (local.y - v.y) / v.scale});
                    return true;
                }
                auto result = p->canvas.input(local);
                if (p->main)
                    p->main->zoom.set(
                        ii::core::format_decimal(std::round(p->canvas.viewport().scale * 100)) +
                        "%");
                return result;
            }
            return false;
        };
        backend.resized = [weak](ii::gui::Rect bounds)
        {
            if (auto p = weak.lock())
                p->canvas.resize(bounds);
        };
        backend.name_changed = [weak, &backend]
        {
            if (auto p = weak.lock())
            {
                p->preferences.set_profile_name(backend.display_name.get());
                p->session.set_profile(p->preferences.profile_name);
                if (p->main)
                    p->main->display_name.set(p->preferences.profile_name);
            }
        };
        backend.brush_changed = [weak, &backend]
        {
            if (auto p = weak.lock())
                p->apply_brush(backend);
        };
        backend.preference_changed = [weak, &backend]
        {
            if (auto p = weak.lock())
            {
                p->preferences.set_start_recent(backend.start_recent.get());
                p->preferences.set_invitations_allowed(backend.can_invite.get());
                p->session.set_invitations(backend.can_invite.get());
                p->preferences.set_discover_nearby(backend.discover.get());
                if (!p->test)
                {
                    if (backend.discover.get())
                        p->discovery.start();
                    else
                        p->discovery.stop();
                }
            }
        };
    }
    void apply_brush(ui::EditorBackend& backend)
    {
        backend.size_label.set(ii::core::format_decimal(std::round(backend.brush_size.get()))+" px");
        backend.flow_label.set(ii::core::format_decimal(std::round(backend.flow.get()*100))+"%");
        backend.opacity_label.set(ii::core::format_decimal(std::round(backend.opacity.get()*100))+"%");
        backend.hardness_label.set(ii::core::format_decimal(std::round(backend.hardness.get()*100))+"%");
        backend.spacing_label.set(ii::core::format_decimal(backend.spacing_pixels.get())+" px");
        backend.spacing_ratio_label.set(ii::core::format_decimal(std::round(backend.spacing.get()*100))+"%");
        backend.pressure_min_label.set(ii::core::format_decimal(std::round(backend.pressure_min.get()*100))+"%");
        backend.pressure_mid_label.set(ii::core::format_decimal(std::round(backend.pressure_mid.get()*100))+"%");
        backend.pressure_max_label.set(ii::core::format_decimal(std::round(backend.pressure_max.get()*100))+"%");
        backend.stabilizer_label.set(ii::core::format_decimal(std::round(backend.stabilizer.get()*100))+"%");
        canvas.settings.spacing=backend.spacing_pixels.get();
        paint_tools();
        canvas.settings.flow = backend.flow.get();
        canvas.settings.hardness = backend.hardness.get();
        canvas.settings.spacingRatio = backend.spacing.get();
        canvas.settings.pressureToOpacityEnabled = backend.pressure_enabled.get();
        BrushState engine;
        engine.rasterizer.smoothingEnabled = backend.stabilizer.get() > 0;
        engine.rasterizer.smoothing = backend.stabilizer.get();
        engine.dynamics.pressureToSize = 1;
        engine.dynamics.pressureToFlow = 1;
        engine.dynamics.pressureToOpacity = 0;
        auto& response = engine.dynamics.opacityResponse;
        response.enabled = backend.pressure_enabled.get();
        response.pressure = {true, std::clamp(backend.pressure_min.get(), 0., 1.),
                             std::clamp(backend.pressure_mid.get(), 0., 1.),
                             std::clamp(backend.pressure_max.get(), 0., 1.)};
        canvas.settings.engineState = std::move(engine);
        if (main && &backend != main)
        {
            main->spacing_pixels.set(backend.spacing_pixels.get());
            main->flow.set(backend.flow.get());
            main->hardness.set(backend.hardness.get());
            main->spacing.set(backend.spacing.get());
            main->stabilizer.set(backend.stabilizer.get());
            main->pressure_enabled.set(backend.pressure_enabled.get());
            main->pressure_min.set(backend.pressure_min.get());
            main->pressure_mid.set(backend.pressure_mid.get());
            main->pressure_max.set(backend.pressure_max.get());
        }
    }
    void fetch_image(std::string url, Point point)
    {
        if (!url.starts_with("https://") && !url.starts_with("http://"))
        {
            status("Choose an HTTP or HTTPS image URL");
            return;
        }
        image_http.cancel();
        ii::network::HttpRequest request;
        request.url = std::move(url);
        request.maximum_bytes = 16 * 1024 * 1024;
        request.follow_redirects = true;
        request.maximum_redirects = 3;
        request.timeout = std::chrono::seconds(15);
        const auto document = artwork.recovery();
        auto weak = weak_from_this();
        if (!image_http.start(
                std::move(request),
                [weak, document, point](auto response)
                {
                    if (auto p = weak.lock(); p && !p->closed)
                    {
                        if (p->artwork.recovery() != document)
                            return;
                        if (!response.ok())
                        {
                            p->status(response.error.empty() ? "The image download failed"
                                                             : response.error);
                            return;
                        }
                        auto decoded = decodeBitmap(
                            {reinterpret_cast<const std::uint8_t*>(response.body.data()),
                             response.body.size()});
                        if (!decoded.ok())
                        {
                            p->status(decoded.result.message);
                            return;
                        }
                        p->result(p->artwork.import_pixels(std::move(decoded.asset.pixels),
                                                           "Dropped web image", point.x, point.y));
                    }
                }))
            status("The image download could not start");
    }
    void draw_avatar()
    {
        if (!preferences_view)
            return;
        auto widget = take(preferences_view->widgets().find("profile-image"));
        ii::gui::WidgetDrawing drawing;
        if (avatar)
            drawing.images.push_back({{0, 0, 64, 64}, avatar});
        else if(auto icon=lvrs::Icons::image("user"))
            drawing.images.push_back({{10,10,44,44},*icon});
        good(widget.set_drawing(std::move(drawing)));
    }
    void set_avatar(const std::filesystem::path& path)
    {
        auto bytes = iiFileProvider::native::File::read(path, 64 * 1024 * 1024);
        auto decoded =
            ii::image::decode({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
        if (!decoded)
        {
            status(decoded.error().message);
            return;
        }
        auto circle = ii::image::thumbnail(decoded->view(), 256, true);
        if (!circle)
        {
            status(circle.error().message);
            return;
        }
        auto converted = ii::image::to_srgb(*circle);
        if (!converted)
        {
            status(converted.error().message);
            return;
        }
        auto png = ii::image::encode_png(circle->view());
        if (!png)
        {
            status(png.error().message);
            return;
        }
        iiFileProvider::native::writeCompatible(
            directory / "profile.png", {reinterpret_cast<const char*>(png->data()), png->size()});
        avatar = take(
            ii::gui::rgba_image(converted->width, converted->height, std::move(converted->rgba)));
        draw_avatar();
        status("Profile photo updated");
    }
    void open_setup()
    {
        if (setup_host)
        {
            auto current = setup_host->snapshot();
            if (current && !current->closed)
            {
                good(setup_host->window_action(ii::gui::WindowAction::raise));
                return;
            }
        }
        setup_view = std::make_shared<CanvasSetupView>();
        auto options = CanvasSetupView::options();
        options.renderer = renderer;
        setup_host = std::make_shared<ii::gui::Host>(options);
        const auto weak = weak_from_this();
        good(setup_host->show(
            *host, {.opened =
                        [weak]
                    {
                        if (auto p = weak.lock())
                        {
                            good(p->setup_view->attach(*p->setup_host,
                                                       [p, weak](CanvasSetupView::Scope& scope)
                                                       {
                                                           p->setup = &scope.backend_0;
                                                           scope.backend_0.status.set("");
                                                           scope.backend_0.perform =
                                                               [weak](Action action)
                                                           {
                                                               if (auto p = weak.lock())
                                                                   p->act(action);
                                                           };
                                                       }));
                            editor::panel(take(p->setup_view->widgets().find("setup-root")),editor::workspace);
                            if (p->test && p->after_setup)
                            {
                                auto call = std::exchange(p->after_setup, {});
                                check(bool(p->host->executor().post(std::move(call))),
                                      "Setup test continuation was rejected");
                            }
                        }
                    },
                    .resized = [weak](ii::gui::Size size) { if(auto p=weak.lock();p&&p->setup_view) if(auto root=p->setup_view->widgets().find("setup-root"))good(root->set_extent(size)); },
                    .closed =
                        [weak]
                    {
                        if (auto p = weak.lock())
                            p->setup = nullptr;
                    }}));
    }

    void start()
    {
        const auto weak = weak_from_this();
        artwork.changed = [weak]
        {
            if (auto p = weak.lock())
                p->changed();
        };
        artwork.selection_changed = [weak]
        {
            if (auto p = weak.lock())
                if (!p->artwork.selected().empty())
                    take(p->layers->select(p->artwork.selected()));
        };
        canvas.status = [weak](std::string message)
        {
            if (auto p = weak.lock())
                p->status(std::move(message));
        };
        color_port = std::make_shared<ii::gui::ColorChangePort>(
            [weak](const ii::gui::ColorPickerChange&)
            {
                if (auto p = weak.lock())
                {
                    auto value = take(p->color->state()).color;
                    p->canvas.color = (std::uint32_t(std::lround(value.alpha * 255)) << 24) |
                                      (std::uint32_t(std::lround(value.red * 255)) << 16) |
                                      (std::uint32_t(std::lround(value.green * 255)) << 8) |
                                      std::uint32_t(std::lround(value.blue * 255));
                    p->paint_tools();
                }
            });
        good(color->observe(color_port));
        good(color->set_color({.1, .1, .1, 1}));
        canvas.color_picked = [weak](std::uint32_t value)
        {
            if (auto p = weak.lock())
            {
                good(p->color->set_color({double((value >> 16) & 255) / 255,
                                          double((value >> 8) & 255) / 255,
                                          double(value & 255) / 255, double(value >> 24) / 255}));
                if (p->main)
                    p->main->tool.set("brush");
                p->status("Canvas color sampled");
            }
        };
        const auto mounted=view->widgets();
        editor::panel(take(mounted.find("root")),editor::workspace);
        editor::panel(take(mounted.find("toolbar")),editor::toolbar);
        editor::panel(take(mounted.find("sidebar")),editor::sidebar);
        editor::panel(take(mounted.find("tool-divider")),{1,1,1,.12});
        auto sidebar=take(mounted.find("sidebar"));
        good(sidebar.set_input_handler([weak,sidebar](const ii::gui::WidgetInput& e)->ii::core::Result<bool>{
            if(auto p=weak.lock();p&&p->main) {
                if(e.kind==ii::gui::WidgetInputKind::pointer_move)
                    {const auto r=take(sidebar.snapshot()).bounds;p->main->layer_controls.set(e.y>=r.y+r.height-28);}
                else if(e.kind==ii::gui::WidgetInputKind::pointer_leave)p->main->layer_controls.set(false);
            }
            return false;
        }));
        ii::gui::WidgetDrawing arrow;
        for(auto xy:{std::pair{4.f,12.f},{5.f,13.f},{6.f,12.f}})
            arrow.panels.push_back({{xy.first,xy.second,1,1},editor::linear({.8,.81,.84,1}),editor::linear({.8,.81,.84,1})});
        good(take(mounted.find("shape-arrow")).set_drawing(std::move(arrow)));
        canvas.attach(take(mounted.find("canvas")));
        refresh();
        good(take(view->widgets().find("root"))
                 .set_input_handler(
                     [weak](const ii::gui::WidgetInput& event) -> ii::core::Result<bool>
                     {
                         if (auto p = weak.lock())
                             return p->shortcut(event);
                         return false;
                     }));
        install_menu();
        apply_brush(*main);
        session.set_profile(preferences.profile_name);
        session.set_invitations(preferences.invitations_allowed);
        if (!test)
        {
            std::error_code ec;
            if (std::filesystem::is_regular_file(directory / "profile.png", ec))
                try
                {
                    set_avatar(directory / "profile.png");
                }
                catch (const std::exception& error)
                {
                    status(error.what());
                }
        }
        license.changed = [weak](LicenseService::Notice)
        {
            if (auto p = weak.lock())
            {
                for (auto* backend : {p->main, p->secondary})
                    if (backend)
                    {
                        backend->licensed.set(p->license.licensed);
                        backend->needs_license.set(!p->license.licensed);
                        backend->can_activate.set(!p->license.verifying);
                    }
                if (!p->license.code.empty())
                    p->status(p->license.code);
            }
        };
        license.validated = [weak](bool valid)
        {
            if (auto p = weak.lock())
            {
                if (valid)
                {
                    p->license.credentials(
                        [weak](auto state, Credentials value)
                        {
                            if (auto p = weak.lock();
                                p && state == LicenseService::StoredStatus::available)
                                {
                                    if(p->main)p->main->email.set(value.email);
                                    if(p->secondary)p->secondary->account_email.set(value.email);
                                    p->account.activate(std::move(value.email), std::move(value.key));
                                }
                        });
                    p->canvas.fit();
                    p->status("License activated");
                }
            }
        };
        updater.setCredentialProvider(&update_credentials);
        updater.upToDate = [weak](std::string version)
        {
            if (auto p = weak.lock())
                p->update_status("Congregation " + version + " is up to date");
        };
        updater.updateAvailable = [weak](std::string version, std::string)
        {
            if (auto p = weak.lock())
                p->update_status("Update available: " + version);
        };
        updater.checkFailed = [weak](auto, std::string message)
        {
            if (auto p = weak.lock())
                p->update_status(std::move(message));
        };
        updater.updateFailed = [weak](auto, std::string message)
        {
            if (auto p = weak.lock())
                p->update_status(std::move(message));
        };
        updater.installerLaunched = [weak](std::string)
        {
            if (auto p = weak.lock())
                good(p->app.quit());
        };
        session.snapshot_requested = [weak]
        {
            if (auto p = weak.lock())
                p->session.publish(p->artwork.snapshot());
        };
        session.snapshot_received = [weak](std::string bytes, std::uint64_t, std::string)
        {
            if (auto p = weak.lock())
            {
                p->receiving = true;
                p->result(p->artwork.receive(bytes));
                p->receiving = false;
            }
        };
        session.edit_received = [weak](Object command, std::string)
        {
            if (auto p = weak.lock())
            {
                if (text(field(command, "type")) == "nativeDocument")
                {
                    auto bytes = unbase64(text(field(command, "document")));
                    if (bytes)
                        p->result(p->artwork.receive(*bytes));
                }
            }
        };
        session.changed = [weak](auto)
        {
            if (auto p = weak.lock())
                p->participants();
        };
        session.advertise = [weak](std::uint16_t port)
        {
            if (auto p = weak.lock())
                p->discovery.set_port(port);
        };
        discovery.changed = [weak]
        {
            if (auto p = weak.lock())
                p->participants();
        };
        discovery.invited = [weak](Object invitation)
        {
            if (auto p = weak.lock())
            {
                p->session.receive_invitation(std::move(invitation));
                p->status("A canvas invitation is waiting in Preferences");
            }
        };
        if (!test)
        {
            license.restore();
            account.loadLicense();
            if (preferences.discover_nearby)
                discovery.start();
            if (preferences.start_recent)
            {
                const auto path = preferences.existing_canvas();
                if (!path.empty())
                    result(artwork.recover(path));
            }
        }
        check(bool(engine.start([] {})), "Core engine startup was rejected");
        if (!initial_file.empty())
        {
            result(artwork.open(initial_file));
            canvas.fit();
            refresh();
        }
        if (test)
        {
            auto submitted = host->executor().post(
                [weak]
                {
                    if (auto p = weak.lock()) {
                        if(p->reference_image.empty())p->self_test();else p->reference_capture();
                    }
                });
            check(bool(submitted), "Self-test admission failed");
        }
    }
    void update_status(std::string message)
    {
        if (main)
            main->update_status.set(message);
        if (secondary)
            secondary->update_status.set(message);
        status(std::move(message));
    }
    void participants()
    {
        std::vector<lvrs::ListRowData> rows;
        rows.push_back({session.peer_id, preferences.profile_name + " (you)", "", false, false});
        for (const auto& item : session.profiles)
        {
            auto profile = object(item);
            rows.push_back({text(field(profile, "peerId")), text(field(profile, "profileName")), "",
                            true, false});
        }
        for (const auto& item : discovery.canvases())
        {
            auto peer = object(item);
            rows.push_back({"nearby-" + text(field(peer, "sessionId")),
                            text(field(peer, "address")) + ":" +
                                std::to_string(int(number(field(peer, "port")))),
                            "", true, false});
        }
        take(members->set_rows(std::move(rows)));
        if (!session.error.empty())
            status(session.error);
    }
    void open_preferences()
    {
        if (preferences_host)
        {
            auto current = preferences_host->snapshot();
            if (current && !current->closed)
            {
                good(preferences_host->window_action(ii::gui::WindowAction::raise));
                return;
            }
            secondary = nullptr;
            preferences_view.reset();
            preferences_host.reset();
        }
        auto options = PreferencesView::options();
        options.renderer = renderer;
        preferences_host = std::make_shared<ii::gui::Host>(options);
        preferences_view = std::make_shared<PreferencesView>();
        auto weak = weak_from_this();
        good(preferences_host->show(
            *host, {.opened =
                        [weak]
                    {
                        if (auto p = weak.lock())
                        {
                            good(p->preferences_view->attach(*p->preferences_host,
                                                             [p](PreferencesView::Scope& scope)
                                                             { p->bind(scope.backend_0, true); }));
                            editor::panel(take(p->preferences_view->widgets().find("preferences-root")),editor::workspace);
                            p->preference_tab(Action::general_tab);
                            p->draw_avatar();
                            if (p->test && p->after_preferences)
                            {
                                auto call = std::exchange(p->after_preferences, {});
                                check(bool(p->host->executor().post(std::move(call))),
                                      "Preferences test continuation was rejected");
                            }
                        }
                    },
                    .resized = [weak](ii::gui::Size size) { if(auto p=weak.lock();p&&p->preferences_view) if(auto root=p->preferences_view->widgets().find("preferences-root"))good(root->set_extent(size)); },
                    .closed =
                        [weak]
                    {
                        if (auto p = weak.lock())
                        {
                            p->secondary = nullptr;
                            auto submitted = p->host->executor().post(
                                [weak]
                                {
                                    if (auto p = weak.lock())
                                    {
                                        p->preferences_view.reset();
                                        p->preferences_host.reset();
                                    }
                                });
                            (void) submitted;
                        }
                    }}));
    }
    void choose(bool export_file, bool save_as = false)
    {
        ii::gui::FileDialogOptions options;
        options.mode =
            export_file || save_as ? ii::gui::FileDialogMode::save : ii::gui::FileDialogMode::open;
        options.title = export_file ? "Export canvas" : save_as ? "Save canvas" : "Open canvas";
        options.filename = export_file ? "Canvas.png" : save_as ? "Canvas.iisc" : "";
        options.filters =
            export_file ? std::vector<ii::gui::FileFilter>{{"Images",
                                                            {"png", "jpg", "jpeg", "bmp", "tga",
                                                             "webp", "tif", "tiff", "psd"}}}
                        : std::vector<ii::gui::FileFilter>{
                              {"Canvas and images",
                               {"vrc", "iisc", "psd", "ora", "png", "jpg", "jpeg", "bmp", "gif",
                                "tga", "webp", "tif", "tiff"}}};
        auto weak = weak_from_this();
        auto result =
            host->choose_file(std::move(options),
                              [weak, export_file, save_as](auto result)
                              {
                                  if (auto p = weak.lock())
                                  {
                                      if (!result)
                                      {
                                          p->status(result.error().message);
                                          return;
                                      }
                                      if (result->canceled || result->paths.empty())
                                          return;
                                      auto path = native_path(result->paths[0]);
                                      if (export_file)
                                      {
                                          auto extension = lower(path.extension().string());
                                          if (!extension.empty())
                                              extension.erase(0, 1);
                                          p->result(p->artwork.export_image(path, extension),
                                                    "Exported " + path_text(path.filename()));
                                      }
                                      else if (save_as)
                                          p->result(p->artwork.save(path), "Canvas saved");
                                      else
                                      {
                                          p->result(p->artwork.open(path));
                                          p->canvas.fit();
                                          p->refresh();
                                      }
                                  }
                              });
        if (!result)
            status(result.error().message);
    }
    ii::core::Result<bool> shortcut(const ii::gui::WidgetInput& event)
    {
        if (event.kind != ii::gui::WidgetInputKind::key_down || event.text_input_active)
            return false;
        if (event.key == ii::gui::WidgetKey::delete_forward ||
            event.key == ii::gui::WidgetKey::backspace)
        {
            act(Action::delete_layer);
            return true;
        }
        if (event.key == ii::gui::WidgetKey::escape)
        {
            if(canvas.presentation) { act(Action::presentation); return true; }
            artwork.cancel();
            canvas.refresh();
            return false;
        }
        std::string key = lower(event.text);
        if (event.meta || event.control)
        {
            if (key == "n")
                act(Action::new_document);
            else if (key == "o")
                act(Action::open);
            else if (key == "s")
                act(event.shift ? Action::save_as : Action::save);
            else if (key == "z")
                act(event.shift ? Action::redo : Action::undo);
            else if (key == "c")
                act(Action::copy);
            else if (key == "v")
                act(Action::paste);
            else if (key == "+" || key == "=")
                act(Action::zoom_in);
            else if (key == "-")
                act(Action::zoom_out);
            else
                return false;
            return true;
        }
        if (key == "b")
            act(Action::brush);
        else if (key == "e")
            act(Action::eraser);
        else if (key == "t")
            act(Action::text);
        else if (key == "v")
            act(Action::select);
        else if (key == "z")
            act(Action::zoom_tool);
        else if (key == "h")
            act(Action::pan);
        else
            return false;
        return true;
    }
    void install_menu()
    {
        auto weak = weak_from_this();
        auto item = [&](std::string id, std::string title, Action action, std::string key = {})
        {
            ii::gui::AppMenuItem value{.id = std::move(id), .title = std::move(title)};
            if (!key.empty())
                value.shortcut = ii::gui::AppShortcut{key};
            value.triggered = [weak, action]
            {
                if (auto p = weak.lock())
                    p->act(action);
            };
            return value;
        };
        good(host->set_app_menu(
            {{.id = "app",
              .title = "Congregation",
              .children = {item("preferences", "Preferences…", Action::preferences, ","),
                           item("quit", "Quit Congregation", Action::quit, "q")}},
             {.id = "file",
              .title = "File",
              .children = {item("new", "New", Action::new_document, "n"),
                           item("open", "Open…", Action::open, "o"),
                           item("save", "Save", Action::save, "s"),
                           item("save-as", "Save As…", Action::save_as),
                           item("export", "Export…", Action::export_document)}},
             {.id = "edit",
              .title = "Edit",
              .children = {item("undo", "Undo", Action::undo, "z"),
                           item("redo", "Redo", Action::redo),
                           item("copy", "Copy canvas", Action::copy),
                           item("paste", "Paste image", Action::paste)}},
             {.id = "canvas",
              .title = "Window",
              .children = {item("fit", "Fit", Action::fit),
                           item("zoom-in", "Zoom In", Action::zoom_in, "+"),
                           item("zoom-out", "Zoom Out", Action::zoom_out, "-"),
                           item("presentation", "Presentation", Action::presentation),
                           item("place-image", "Place image…", Action::image),
                           item("line", "Draw line", Action::line)}}}));
    }
    void act(Action action)
    {
        if (action == Action::undo || action == Action::redo || action == Action::copy ||
            action == Action::paste)
        {
            for (const auto& current :
                 {view->widgets(),
                  preferences_view ? preferences_view->widgets() : ii::gui::WidgetMount{}})
            {
                auto snapshots = current.snapshot();
                if (snapshots)
                    for (const auto& widget : *snapshots)
                        if (widget.focused && widget.kind == ii::gui::WidgetKind::input_field)
                            return;
            }
        }
        if (action == Action::quit)
        {
            good(app.quit());
            return;
        }
        if (action == Action::preferences)
        {
            open_preferences();
            return;
        }
        if (action == Action::activate)
        {
            if (main)
            {
                license.validate(main->email.get(), main->license_key.get());
                main->license_key.set("");
            }
            return;
        }
        if (action == Action::restore_license) { license.restore(); status("Restoring purchases"); return; }
        if (action == Action::forget_license)
        {
            license.forget();
            return;
        }
        if (action == Action::profile)
        {
            good(ii::gui::open_external_url("https://iisacc.com/account"));
            return;
        }
        if (!test && !license.licensed)
        {
            status("Activate a Congregation license to edit this canvas");
            return;
        }
        switch (action)
        {
        case Action::close_notice:
            (void)notice_deadline.cancel();
            good(take(view->widgets().find("status-notice")).close_popup());break;
        case Action::toggle_layer_visibility:
            for(const auto& layer:artwork.document().layers)if(layerProperties(layer).id==artwork.selected()) {
                const auto key=artwork.selected();const bool visible=layerProperties(layer).visible;
                result(artwork.visible(key,!visible));break;
            }
            break;
        case Action::open_join:
            if(preferences_view) {
                auto mount=preferences_view->widgets();auto anchor=take(mount.find("join-session"));
                good(take(mount.find("join-popup")).open_for(anchor,0,take(anchor.snapshot()).bounds.height));
            }
            break;
        case Action::general_tab: case Action::profile_tab: case Action::members_tab:
            preference_tab(action);break;
        case Action::open_avatar:
            if(preferences_view) {
                auto mount=preferences_view->widgets();auto anchor=take(mount.find("profile-image"));
                good(take(mount.find("avatar-menu")).open_for(anchor,0,64));
            }
            break;
        case Action::size_minus: main->set_brush_size(std::max(1.,main->brush_size.get()-1));break;
        case Action::size_plus: main->set_brush_size(std::min(48.,main->brush_size.get()+1));break;
        case Action::open_shapes: popup("shape-menu","shape");break;
        case Action::zoom_tool: canvas.tool="zoom";break;
        case Action::presentation:
            canvas.presentation=!canvas.presentation;
            main->chrome_visible.set(!canvas.presentation);
            good(host->window_action(ii::gui::WindowAction::toggle_fullscreen));
            break;
        case Action::remove_member:
            if (session.state != CanvasSession::State::hosting || selected_member.empty() ||
                !session.remove(selected_member))
                status("Select a connected member while hosting this canvas");
            break;
        case Action::eyedropper:
            canvas.tool = "eyedropper";
            good(take(view->widgets().find("color-popup")).close_popup());
            status("Click the canvas to sample a color");
            break;
        case Action::open_color:
        {
            auto mount = view->widgets();
            auto anchor = take(mount.find("brush-color"));
            good(take(mount.find("color-popup"))
                     .open_for(anchor, 0, take(anchor.snapshot()).bounds.height));
            break;
        }
        case Action::close_color:
            good(take(view->widgets().find("color-popup")).close_popup());
            break;
        case Action::new_document:
            open_setup();
            break;
        case Action::cancel_canvas:
            if (setup_host)
                good(setup_host->request_close());
            break;
        case Action::create_canvas:
        {
            if (!setup)
                break;
            auto dimension = [](const std::string& text)
            {
                int value = 0;
                auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
                return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? value
                                                                                           : 0;
            };
            if (!artwork.create(dimension(setup->new_width.get()),
                                dimension(setup->new_height.get()), setup->infinite.get()))
            {
                setup->status.set(artwork.error());
                break;
            }
            canvas.fit();
            good(setup_host->request_close());
            break;
        }
        case Action::choose_profile:
        {
            ii::gui::FileDialogOptions options;
            options.title = "Choose profile photo";
            options.filters = {{"Images", {"png", "jpg", "jpeg", "bmp", "webp", "tif", "tiff"}}};
            auto weak = weak_from_this();
            auto picker = preferences_host ? preferences_host : host;
            good(picker->choose_file(std::move(options),
                                     [weak](auto chosen)
                                     {
                                         if (auto p = weak.lock())
                                         {
                                             if (!chosen)
                                             {
                                                 p->status(chosen.error().message);
                                                 return;
                                             }
                                             if (chosen->canceled || chosen->paths.empty())
                                                 return;
                                             p->set_avatar(native_path(chosen->paths.front()));
                                         }
                                     }));
            break;
        }
        case Action::clear_profile:
        {
            avatar.reset();
            std::error_code ec;
            if (std::filesystem::exists(directory / "profile.png", ec))
                (void) iiFileProvider::native::File::remove(directory / "profile.png");
            draw_avatar();
            break;
        }
        case Action::diamond:
            canvas.tool = "diamond";
            break;
        case Action::star:
            canvas.tool = "star";
            break;
        case Action::rectanglebubble:
            canvas.tool = "rectanglebubble";
            break;
        case Action::ellipsebubble:
            canvas.tool = "ellipsebubble";
            break;
        case Action::open:
            choose(false);
            break;
        case Action::save:
            if (artwork.source().empty())
                choose(false, true);
            else
                result(artwork.save(artwork.source()), "Canvas saved");
            break;
        case Action::save_as:
            choose(false, true);
            break;
        case Action::export_document:
            choose(true);
            break;
        case Action::undo:
            result(artwork.undo());
            break;
        case Action::redo:
            result(artwork.redo());
            break;
        case Action::add_layer:
            result(artwork.add_layer());
            break;
        case Action::duplicate_layer:
            result(artwork.duplicate_layer());
            break;
        case Action::delete_layer:
            result(artwork.remove_layer());
            break;
        case Action::brush:
            if(canvas.tool=="brush")popup("brush-popup","brush");
            canvas.tool = "brush";
            break;
        case Action::eraser:
            canvas.tool = "eraser";
            break;
        case Action::line:
            canvas.tool = "line";
            break;
        case Action::rectangle:
            canvas.tool = "rectangle";
            break;
        case Action::ellipse:
            canvas.tool = "ellipse";
            break;
        case Action::triangle:
            canvas.tool = "triangle";
            break;
        case Action::fill:
            canvas.tool = "fill";
            break;
        case Action::text:
            canvas.tool = "text";
            popup("text-popup","text");
            break;
        case Action::select:
            canvas.tool = "select";
            break;
        case Action::pan:
            canvas.tool = "pan";
            break;
        case Action::zoom_in:
            canvas.zoom(1.25);
            break;
        case Action::zoom_out:
            canvas.zoom(.8);
            break;
        case Action::fit:
            canvas.fit();
            break;
        case Action::image:
        {
            ii::gui::FileDialogOptions options;
            options.title = "Place image";
            options.filters = {
                {"Images", {"png", "jpg", "jpeg", "bmp", "gif", "tga", "webp", "tif", "tiff"}}};
            auto weak = weak_from_this();
            good(host->choose_file(
                std::move(options),
                [weak](auto result)
                {
                    if (auto p = weak.lock();
                        p && result && !result->canceled && !result->paths.empty())
                        p->result(p->artwork.import_image(native_path(result->paths[0])));
                }));
            break;
        }
        case Action::copy:
        {
            auto rendered = renderFrame(artwork.document(), 0);
            if (!rendered.ok())
            {
                status(rendered.message);
                break;
            }
            auto png = encodeBitmap(rendered.pixels);
            if (!png.ok())
            {
                status(png.result.message);
                break;
            }
            good(ii::gui::write_clipboard(
                {.image_mime = "image/png", .image = std::move(png.bytes)}));
            status("Canvas copied");
            break;
        }
        case Action::paste:
        {
            auto content = take(ii::gui::read_clipboard());
            if (!content.image.empty())
            {
                auto decoded = decodeBitmap(content.image);
                if (decoded.ok())
                    result(artwork.import_pixels(std::move(decoded.asset.pixels), "Pasted image", 0,
                                                 0));
                else
                    status(decoded.result.message);
            }
            for (const auto& path : content.files)
                result(artwork.import_image(native_path(path)));
            break;
        }
        case Action::check_update:
            if (!updater.checkForUpdates())
                update_status("An update check is already running");
            break;
        case Action::install_update:
            if (!updater.updateNow())
                update_status("Check for an available update first");
            break;
        case Action::host_session:
            if (session.host(secondary ? secondary->display_name.get() : "Host"))
            {
                session.set_invitations(true);
                session.publish(artwork.snapshot());
                status("Canvas sharing is active on port " + std::to_string(session.port()));
            }
            else
                status(session.error);
            break;
        case Action::join_session:
        {
            const auto input = secondary ? secondary->session_address.get() : "";
            auto last = input.rfind(':');
            if (last == input.npos)
            {
                status("Enter session-id@host:port");
                break;
            }
            auto at = input.find('@');
            if (at == input.npos)
            {
                status("Enter session-id@host:port");
                break;
            }
            int port = 0;
            auto part = std::string_view(input).substr(last + 1);
            auto parsed = std::from_chars(part.data(), part.data() + part.size(), port);
            if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size() || at >= last ||
                port < 1 || port > 65535)
            {
                status("Invitation port is invalid");
                break;
            }
            if (!session.join(input.substr(0, at), input.substr(at + 1, last - at - 1), port,
                              secondary->display_name.get()))
                status(session.error);
            else if(preferences_view)good(take(preferences_view->widgets().find("join-popup")).close_popup());
            break;
        }
        case Action::leave_session:
            session.stop();
            status("Canvas sharing stopped");
            break;
        case Action::invite:
            if (session.state == CanvasSession::State::hosting)
            {
                std::string lan = "127.0.0.1";
                for (const auto& item : ii::network::interfaces())
                    if (!item.loopback && item.family == ii::network::AddressFamily::ipv4 &&
                        ii::network::local_address(item.address))
                    {
                        lan = item.address;
                        break;
                    }
                auto address = session.session + "@" + lan + ":" + std::to_string(session.port());
                good(ii::gui::write_clipboard({.text = address}));
                status("Invitation copied");
            }
            else
                status("Host a canvas before copying an invitation");
            break;
        case Action::accept_invite:
            if (!session.pending.empty())
            {
                auto invite = object(session.pending.front());
                session.respond(true, text(field(invite, "id")));
            }
            break;
        case Action::reject_invite:
            if (!session.pending.empty())
            {
                auto invite = object(session.pending.front());
                session.respond(false, text(field(invite, "id")));
            }
            break;
        default:
            break;
        }
        if (main)
        {
            main->title.set(artwork.source().empty() ? "Untitled"
                                                     : path_text(artwork.source().filename()));
            main->tool.set(canvas.tool);
            paint_tools();
            main->zoom.set(ii::core::format_decimal(std::round(canvas.viewport().scale * 100)) +
                           "%");
        }
    }
    void popup(std::string_view id,std::string_view anchor_id)
    {
        auto mount=view->widgets();auto anchor=take(mount.find(anchor_id));
        good(take(mount.find(id)).open_for(anchor,0,take(anchor.snapshot()).bounds.height));
    }
    void paint_tools()
    {
        if(!main)return;
        auto mount=view->widgets();
        if(auto curve=mount.find("pressure-curve"))editor::pressure_curve(*curve,main->pressure_min.get(),main->pressure_mid.get(),main->pressure_max.get());
        if(auto preview=mount.find("brush-preview"))
            editor::brush_preview(*preview,main->brush_size.get(),take(color->state()).color);
        for(auto [id,tool]:{std::pair{"pan","pan"},{"select","select"},{"zoom-tool","zoom"},{"brush","brush"},{"eraser","eraser"},{"fill","fill"},{"text","text"}})
            if(auto widget=mount.find(id))editor::tool_button(*widget,canvas.tool==tool);
        if(auto widget=mount.find("shape")) {
            const bool selected=canvas.tool=="line"||canvas.tool=="rectangle"||canvas.tool=="ellipse"||canvas.tool=="triangle"||canvas.tool=="diamond"||canvas.tool=="star"||canvas.tool=="rectanglebubble"||canvas.tool=="ellipsebubble";
            editor::tool_button(*widget,selected);
            if(selected) {
                const auto icon=canvas.tool=="rectanglebubble"?"rectangle":canvas.tool=="ellipsebubble"?"ellipse":canvas.tool=="line"?"showCode":canvas.tool.c_str();
                good(widget->set_button_icon(editor::icon(icon),icon));
            }
        }
    }
    void preference_tab(Action action)
    {
        if(!secondary||!preferences_view)return;
        secondary->general_visible.set(action==Action::general_tab);
        secondary->profile_visible.set(action==Action::profile_tab);
        secondary->members_visible.set(action==Action::members_tab);
        auto mount=preferences_view->widgets();
        for(auto [id,tab]:{std::pair{"general-tab",Action::general_tab},{"profile-tab",Action::profile_tab},{"members-tab",Action::members_tab}})
            editor::tool_button(take(mount.find(id)),tab==action);
        draw_avatar();
    }
    void capture(ii::gui::Host& window,std::string name)
    {
        auto image=take(window.capture_canvas());
        auto png=ii::image::encode_png({image.width,image.height,ii::image::PixelFormat::rgba8,0,image.rgba});
        check(bool(png),"Cannot encode UI reference capture");
        iiFileProvider::native::File::create(directory/name,{reinterpret_cast<const char*>(png->data()),png->size()});
    }
    void reference_capture()
    {
        check(test&&!reference_image.empty(),"Reference capture must use isolated self-test storage");
        const auto bytes=iiFileProvider::native::File::read(reference_image,64*1024*1024);
        auto decoded=take(ii::image::decode({reinterpret_cast<const std::uint8_t*>(bytes.data()),bytes.size()}));
        auto image=take(ii::image::to_srgb(decoded));
        auto pixels=makeRasterLayer(image.width,image.height,0);
        for(std::size_t i=0;i<pixels.pixels.size();++i) {
            const auto* c=image.rgba.data()+i*4;
            pixels.pixels[i]=(std::uint32_t(c[3])<<24)|(std::uint32_t(c[0])<<16)|(std::uint32_t(c[1])<<8)|c[2];
        }
        check(artwork.create(image.width,image.height),artwork.error());
        check(artwork.rename(artwork.selected(),"Background"),artwork.error());
        check(artwork.import_pixels(std::move(pixels),"Layer 1",0,0),artwork.error());
        canvas.fit();refresh();paint_tools();
        auto mount=view->widgets();
        const auto canvas_bounds=take(take(mount.find("canvas")).snapshot()).bounds;
        const auto toolbar_bounds=take(take(mount.find("toolbar")).snapshot()).bounds;
        capture(*host,"editor.png");
        iiFileProvider::native::File::create(directory/"layout.json", "{\"canvasX\":"+std::to_string(canvas_bounds.x)+",\"canvasY\":"+std::to_string(canvas_bounds.y)+",\"canvasWidth\":"+std::to_string(canvas_bounds.width)+",\"canvasHeight\":"+std::to_string(canvas_bounds.height)+",\"toolbarHeight\":"+std::to_string(toolbar_bounds.height)+",\"paperX\":"+std::to_string(canvas.viewport().x)+",\"paperY\":"+std::to_string(canvas.viewport().y)+",\"scale\":"+std::to_string(canvas.viewport().scale)+"}\n");
        check(canvas_bounds.x==editor::sidebar_width&&toolbar_bounds.height==editor::toolbar_height,"Vincent editor chrome geometry changed");
        for(auto id:{"new","open","save","pan","select","zoom-tool","brush","eraser","shape","fill","text"}) {
            const auto button=take(take(mount.find(id)).snapshot());
            check(button.text.empty()&&!button.accessibility_name.empty()&&button.bounds.width==18,"Vincent toolbar requires compact named icon buttons");
        }
        check(take(take(mount.find("brush-size")).snapshot()).bounds.width==160,"Vincent size slider width changed");
        const auto revision=artwork.revision();
        good(mount.dispatch_input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=8,.y=850}));
        check(main->layer_controls.get(),"Layer footer did not appear on hover");
        good(mount.dispatch_input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=270,.y=50}));
        check(!main->layer_controls.get(),"Layer footer did not hide after leaving");
        popup("brush-popup","brush");capture(*host,"brush.png");
        const auto curve=take(take(mount.find("pressure-curve")).snapshot()).bounds;
        good(mount.dispatch_input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=curve.x+curve.width/2,.y=curve.y+46,.buttons=1}));
        good(mount.dispatch_input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=curve.x+curve.width/2,.y=curve.y+6,.buttons=1}));
        good(mount.dispatch_input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=curve.x+curve.width/2,.y=curve.y+6}));
        check(main->pressure_mid.get()==1&&artwork.revision()==revision,"Pressure graph editing failed: midpoint="+std::to_string(main->pressure_mid.get())+" revision="+std::to_string(artwork.revision())+" expected="+std::to_string(revision));
        main->set_pressure_mid(.5);
        good(take(mount.find("brush-popup")).close_popup());
        popup("color-popup","brush-color");capture(*host,"color.png");good(take(mount.find("color-popup")).close_popup());
        popup("shape-menu","shape");capture(*host,"shapes.png");
        good(take(mount.find("ellipse")).activate());
        check(take(take(mount.find("shape")).snapshot()).icon_name=="ellipse"&&artwork.revision()==revision,"Shape selection did not replace its toolbar icon");
        act(Action::brush);
        good(take(mount.find("shape-menu")).close_popup());
        auto weak=weak_from_this();
        after_preferences=[weak,revision] {
            if(auto p=weak.lock()) {
                const auto root=take(take(p->preferences_view->widgets().find("preferences-root")).snapshot()).bounds;
                check(root.width==480&&root.height==360,"Preferences must fill the 480×360 client area");
                p->capture(*p->preferences_host,"preferences-general.png");
                p->preference_tab(Action::profile_tab);p->capture(*p->preferences_host,"preferences-profile.png");
                p->preference_tab(Action::members_tab);p->capture(*p->preferences_host,"preferences-members.png");
                check(p->artwork.revision()==revision,"UI presentation changed document data");
                good(p->preferences_host->request_close());
                p->after_setup=[weak,revision] {
                    if(auto p=weak.lock()) {
                        p->capture(*p->setup_host,"new-canvas.png");
                        good(p->setup_host->request_close());
                        p->main->licensed.set(false);p->main->needs_license.set(true);
                        p->capture(*p->host,"activation.png");
                        auto mount=p->view->widgets();
                        const auto email=take(take(mount.find("license-email")).snapshot()).bounds;
                        const auto key=take(take(mount.find("license-key")).snapshot()).bounds;
                        check(key.y>=email.y+email.height,"Activation fields overlap");
                        check(p->artwork.revision()==revision,"Auxiliary UI changed document data");
                        std::cout<<"CONGREGATION_VINCENT_UI_OK toolbar=40 sidebar=260 preferences=480x360 qt=false captures="<<path_text(p->directory)<<'\n';
                        good(p->app.quit());
                    }
                };
                p->open_setup();
            }
        };
        open_preferences();
    }
    void self_test()
    {
        check(test && main, "Isolated self-test state missing");
        const auto mount = view->widgets();
        check(take(host->snapshot()).visible, "Native window is not visible");
        check(&engine.events() == &app.event_loop(), "Engine created a second Core event loop");
#if defined(__APPLE__)
        constexpr auto primary_api = ii::gui::GraphicsApi::metal;
#elif defined(_WIN32)
        constexpr auto primary_api = ii::gui::GraphicsApi::direct3d11;
#else
        constexpr auto primary_api = ii::gui::GraphicsApi::vulkan;
#endif
        check(take(host->graphics_info()).api == (renderer == ii::gui::Renderer::vulkan
                                                      ? ii::gui::GraphicsApi::vulkan
                                                      : primary_api),
              "Requested renderer was not used");
        const auto original = artwork.snapshot();
        result(artwork.open(directory/"missing-ui-test.iisc"));
        check(take(take(mount.find("status-notice")).snapshot()).opened&&artwork.snapshot()==original,"Rejected file open must report an error without altering the canvas");
        act(Action::close_notice);
        auto weak = weak_from_this();
        after_setup = [weak, original]
        {
            if (auto p = weak.lock())
                p->self_test_editor(original);
        };
        auto timeout = host->executor().after(
            std::chrono::seconds(240),
            [] { throw std::runtime_error("Native self-test did not finish"); });
        check(bool(timeout), "Self-test timeout admission failed");
        deadline = timeout.token;
        good(take(mount.find("new")).activate());
    }
    void self_test_editor(const std::string& original)
    {
        const auto mount = view->widgets();
        check(setup && setup_host, "Native canvas setup did not open");
        setup->set_new_width("0");
        check(!take(take(setup_view->widgets().find("create-canvas")).snapshot()).enabled,"Invalid canvas size did not disable Create");
        act(Action::create_canvas);
        check(artwork.snapshot() == original && !take(setup_host->snapshot()).closed,
              "Rejected canvas setup changed the document");
        setup->set_new_width("128");
        setup->set_new_height("96");
        check(take(take(setup_view->widgets().find("create-canvas")).snapshot()).enabled,"Valid canvas size did not enable Create");
        act(Action::create_canvas);
        check(artwork.document().extent == CanvasExtent{128, 96} &&
                  take(setup_host->snapshot()).closed,
              "Native canvas setup failed");
        good(take(mount.find("brush-color")).activate());
        check(take(take(mount.find("color-popup")).snapshot()).opened,
              "Native color picker did not open");
        take(host->capture_canvas());
        good(take(mount.find("color-popup")).close_popup());
        good(take(mount.find("brush")).activate());
        good(take(mount.find("brush-popup")).close_popup());
        main->brush_size.set(12);
        good(color->set_color({.1, .2, 1, 1}));
        const auto revision = artwork.revision();
        auto bounds = take(take(mount.find("canvas")).snapshot()).bounds;
        auto point = [&](double x, double y, ii::gui::WidgetInputKind kind)
        {
            auto v = canvas.viewport();
            return ii::gui::WidgetInput{.kind = kind,
                                        .x = bounds.x + v.x + x * v.scale,
                                        .y = bounds.y + v.y + y * v.scale,
                                        .device = ii::gui::PointerDevice::pen,
                                        .pressure = .5,
                                        .device_id = 42,
                                        .buttons = 1};
        };
        good(mount.dispatch_input(point(12, 12, ii::gui::WidgetInputKind::pointer_down)));
        good(mount.dispatch_input(point(35, 24, ii::gui::WidgetInputKind::pointer_move)));
        good(mount.dispatch_input(point(48, 30, ii::gui::WidgetInputKind::pointer_up)));
        check(artwork.revision() == revision + 1,
              "Native pen stroke did not produce one durable edit");
        auto pixels = renderFrame(artwork.document(), 0).pixels.pixels;
        check(std::ranges::any_of(pixels, [](auto p) { return p >> 24; }),
              "Native canvas did not paint");
        const auto stroke_pixels=renderFrame(artwork.document(),0).pixels;
        check((stroke_pixels.pixels[12*stroke_pixels.width+12]>>24)>0,"Surface coordinates were not mapped to the canvas stroke origin");
        auto canceled = artwork.snapshot();
        auto canceled_revision = artwork.revision();
        good(mount.dispatch_input(point(60, 35, ii::gui::WidgetInputKind::pointer_down)));
        good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::focus_lost}));
        check(artwork.snapshot() == canceled && artwork.revision() == canceled_revision,
              "Native focus cancellation persisted preview pixels");
        popup("text-popup","text");
        good(take(mount.find("text-value")).focus());
        act(Action::undo);
        check(artwork.snapshot() == canceled, "Text editing menu action undid the canvas");
        good(take(mount.find("text-popup")).close_popup());
        good(take(mount.find("brush")).focus());
        auto saved = artwork.snapshot();
        const auto before_pan = artwork.revision();
        good(take(mount.find("pan")).activate());
        auto down = point(20, 20, ii::gui::WidgetInputKind::pointer_down);
        good(mount.dispatch_input(down));
        down.kind = ii::gui::WidgetInputKind::pointer_move;
        down.x += 30;
        down.y += 12;
        good(mount.dispatch_input(down));
        down.kind = ii::gui::WidgetInputKind::pointer_up;
        good(mount.dispatch_input(down));
        act(Action::zoom_in);
        check(artwork.revision() == before_pan && artwork.snapshot() == saved,
              "Navigation changed saved artwork");
        const auto before_gesture = canvas.viewport().scale;
        good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::key_down,
                                   .key = ii::gui::WidgetKey::space,
                                   .control = true}));
        auto gesture = point(20, 20, ii::gui::WidgetInputKind::pointer_down);
        gesture.control = true;
        good(mount.dispatch_input(gesture));
        gesture.kind = ii::gui::WidgetInputKind::pointer_move;
        gesture.x += 20;
        good(mount.dispatch_input(gesture));
        gesture.kind = ii::gui::WidgetInputKind::pointer_up;
        good(mount.dispatch_input(gesture));
        good(mount.dispatch_input(
            {.kind = ii::gui::WidgetInputKind::key_up, .key = ii::gui::WidgetKey::space}));
        check(canvas.viewport().scale != before_gesture && artwork.snapshot() == saved,
              "Temporary zoom changed saved artwork or did not zoom");
        good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::key_down,
                                   .key = ii::gui::WidgetKey::character,
                                   .text = "z",
                                   .meta = true}));
        check(renderFrame(artwork.document(), 0).pixels.pixels != pixels,
              "Native Undo action failed");
        good(mount.dispatch_input({.kind = ii::gui::WidgetInputKind::key_down,
                                   .key = ii::gui::WidgetKey::character,
                                   .shift = true,
                                   .text = "z",
                                   .meta = true}));
        check(renderFrame(artwork.document(), 0).pixels.pixels == pixels,
              "Native Redo action failed");
        act(Action::add_layer);
        check(artwork.document().layers.size() == 2, "Layer action failed");
        result(artwork.rename(artwork.selected(), "검증 레이어"));
        check(layers->rows().front().text == "검증 레이어", "Reactive layer list did not update");
        auto placed = encodeBitmap(makeRasterLayer(2, 2, 0xff22cc66));
        check(placed.ok(), placed.result.message);
        auto dropped = std::make_shared<ii::gui::ClipboardContent>();
        dropped->image_mime = "image/png";
        dropped->image = std::move(placed.bytes);
        auto drop = point(90, 70, ii::gui::WidgetInputKind::drop);
        drop.content = std::move(dropped);
        good(mount.dispatch_input(drop));
        check(layerProperties(artwork.document().layers.back()).name == "Dropped image",
              "Native image drop was not placed");
        main->text_value.set("한글\nNative");
        act(Action::text);
        good(take(mount.find("text-popup")).close_popup());
        good(mount.dispatch_input(point(8, 65, ii::gui::WidgetInputKind::pointer_down)));
        check(layerProperties(artwork.document().layers.back()).name.starts_with("Text: 한글"),
              "Native multiline text was not placed");
        const auto picked_revision = artwork.revision();
        canvas.tool = "eyedropper";
        good(mount.dispatch_input(point(90, 70, ii::gui::WidgetInputKind::pointer_down)));
        check(artwork.revision() == picked_revision && canvas.tool == "brush",
              "Color sampling modified the saved document");
        check(artwork.shape("ellipse", {20, 20}, {70, 60}, 0xffee4400, 2, false), artwork.error());
        check(artwork.save(directory / "native.iisc"), artwork.error());
        check(artwork.export_image(directory / "native.png", "png"), artwork.error());
        check(artwork.export_image(directory / "native.psd", "psd"), artwork.error());
        auto expected = renderFrame(artwork.document(), 0).pixels.pixels;
        check(artwork.open(directory / "native.iisc"), artwork.error());
        check(renderFrame(artwork.document(), 0).pixels.pixels == expected,
              "Save/reopen changed final pixels");
        refresh();
        auto weak = weak_from_this();
        after_preferences = [weak]
        {
            if (auto p = weak.lock())
                p->self_test_finish();
        };
        act(Action::preferences);
    }
    void self_test_finish()
    {
        check(bool(preferences_host) && secondary, "Secondary native window did not open");
        const auto preferences_revision = artwork.revision();
        secondary->set_name("Native Artist 한글");
        secondary->set_flow(.7);
        secondary->set_stabilizer(.2);
        set_avatar(directory / "native.png");
        check(avatar && avatar->rgba()[3] == 0 &&
                  preferences.profile_name == "Native Artist 한글" && canvas.settings.flow == .7,
              "Native profile or brush preferences failed");
        check(artwork.revision() == preferences_revision, "Preferences changed saved artwork");
        good(preferences_host->request_close());
        const auto used_api = take(host->graphics_info()).api;
        auto capture = take(host->capture_canvas());
        auto png = ii::image::encode_png(
            {capture.width, capture.height, ii::image::PixelFormat::rgba8, 0, capture.rgba});
        check(bool(png), "Cannot encode native evidence image");
        iiFileProvider::native::File::create(
            directory / (used_api == ii::gui::GraphicsApi::vulkan       ? "vulkan.png"
                         : used_api == ii::gui::GraphicsApi::direct3d11 ? "direct3d11.png"
                                                                        : "metal.png"),
            {reinterpret_cast<const char*>(png->data()), png->size()});
        std::cout << "CONGREGATION_NATIVE_OK qt=false lvui=html-inline lvrs=true core_loop=shared "
                     "pen=true cancel=true undo=true layers=true navigation_readonly=true "
                     "save_reopen=true png=true psd=true secondary=true setup=true popup=true "
                     "profile=true image_drop=true text=true eyedropper=true "
                     "text_shortcut_guard=true renderer="
                  << (used_api == ii::gui::GraphicsApi::vulkan       ? "Vulkan"
                      : used_api == ii::gui::GraphicsApi::direct3d11 ? "Direct3D11"
                                                                     : "Metal")
                  << " tiles=" << canvas.viewport().visible_tiles << '\n';
        good(app.quit());
    }
};
} // namespace congregation::native

ii::core::Result<void> ii::runtime::configure_application(Application& app)
{
    using namespace congregation::native;
    bool test = false;
    std::filesystem::path directory, initial_file, reference_image;
    ii::gui::Renderer renderer = ii::gui::Renderer::automatic;
    for (std::size_t i = 1; i < app.arguments().size(); ++i)
    {
        const auto& arg = app.arguments()[i];
        if (arg == "--self-test" && i + 1 < app.arguments().size())
        {
            test = true;
            directory = native_path(app.arguments()[++i]);
        }
        else if(arg == "--ui-reference" && i+1<app.arguments().size()) reference_image=native_path(app.arguments()[++i]);
        else if (arg == "--renderer" && i + 1 < app.arguments().size())
        {
            const auto& value = app.arguments()[++i];
            if (value == "vulkan")
                renderer = ii::gui::Renderer::vulkan;
            else if (value == "native")
                renderer = ii::gui::Renderer::native;
            else if (value == "auto")
                renderer = ii::gui::Renderer::automatic;
            else
                return core::fail(core::ErrorCode::invalid_argument,
                                  "Renderer must be auto, native or vulkan");
        }
        else if (!arg.starts_with("-") && initial_file.empty())
            initial_file = native_path(arg);
        else
            return core::fail(core::ErrorCode::invalid_argument,
                              "Usage: Congregation [document] [--renderer native|vulkan] "
                              "[--self-test existing-directory] [--ui-reference image]");
    }
    if(!reference_image.empty()&&!test) return core::fail(core::ErrorCode::invalid_argument,"UI reference capture requires --self-test");
    if (test)
    {
        if (directory.empty() || !std::filesystem::is_directory(directory))
            return core::fail(core::ErrorCode::invalid_argument,
                              "Self-test requires an existing isolated directory");
        auto child = directory / ("congregation-" + congregation::uuid());
        iiFileProvider::native::File::createDirectories(child);
        directory = std::move(child);
    }
    else
        directory = congregation::application_data();
    auto state = std::make_shared<State>(app, test, directory);
    state->renderer = renderer;
    state->reference_image=std::move(reference_image);
    state->initial_file = std::move(initial_file);
    auto weak = std::weak_ptr<State>(state);
    auto options = MainView::options();
    options.renderer = renderer;
    state->host = take(
        gui::attach_application(app, options,
                                {.opened =
                                     [weak]
                                 {
                                     if (auto p = weak.lock())
                                         p->start();
                                 },
                                 .resized = [weak](ii::gui::Size size) { if(auto p=weak.lock();p&&p->view) if(auto root=p->view->widgets().find("root"))good(root->set_extent(size)); },
                                 .close_requested =
                                     [weak]
                                 {
                                     if (auto p = weak.lock())
                                         p->artwork.cancel();
                                     return true;
                                 },
                                 .closed =
                                     [weak]
                                 {
                                     if (auto p = weak.lock())
                                     {
                                         p->closed = true;
                                         p->session.stop();
                                         p->discovery.stop();
                                         p->updater.cancelUpdate();
                                         p->image_http.cancel();
                                     }
                                 },
                                 .files_opened =
                                     [weak](std::vector<std::string> paths)
                                 {
                                     if (auto p = weak.lock(); p && !p->test && !paths.empty())
                                     {
                                         p->result(p->artwork.open(native_path(paths.front())));
                                         p->canvas.fit();
                                         p->refresh();
                                     }
                                 }}));
    auto cleanup =
        app.attach({"native-state", [](Application&) -> core::Result<void> { return {}; },
                    [state]() -> core::Result<void>
                    {
                        state->main = nullptr;
                        state->secondary = nullptr;
                        state->engine.stop();
                        if (state->test && !state->closed)
                            return core::fail(core::ErrorCode::failed, "Native host did not close");
                        return {};
                    }});
    if (!cleanup)
        return cleanup;
    return app.attach(gui::view_attachment(
        "main-lvui", state->host, state->view,
        std::function<void(MainView::Scope&)>{[weak](MainView::Scope& scope)
                                              {
                                                  if (auto p = weak.lock())
                                                      p->bind(scope.backend_0, false);
                                              }}));
}
