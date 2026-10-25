#include "AssetLibraryView.h"
#include "Artwork.h"
#include "EditorLayout.h"
#include "Style.h"
#include "WidgetStyle.h"
#include <algorithm>
#include <iiFileProviderNative.h>
#include <iiImageCodec/ImageCodec.h>

namespace congregation::native
{
namespace
{
std::string lower(std::string v)
{
    std::ranges::transform(v, v.begin(), [](unsigned char c)
                           { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : char(c); });
    return v;
}
std::shared_ptr<const ii::gui::RasterImage> load_image(const std::filesystem::path &path)
{
    auto data = iiFileProvider::native::File::read(path, 32 * 1024 * 1024);
    auto decoded =
        ii::image::decode({reinterpret_cast<const std::uint8_t *>(data.data()), data.size()});
    if (!decoded)
        throw std::runtime_error(decoded.error().message);
    auto srgb = ii::image::to_srgb(*decoded);
    if (!srgb)
        throw std::runtime_error(srgb.error().message);
    return widgets::take(ii::gui::rgba_image(srgb->width, srgb->height, std::move(srgb->rgba)));
}
} // namespace
AssetLibraryView::AssetLibraryView(std::shared_ptr<congregation::ui::EditorBackend> vm,
                                   std::filesystem::path assets)
    : vm_(std::move(vm)), bundled_(std::move(assets))
{
}
ii::gui::Widget AssetLibraryView::build(ii::gui::WidgetBuilder &raw, ii::gui::Widget parent)
{
    using namespace widgets;
    Builder b(raw, "library/");
    auto viewport = take(raw.scroll_view(parent, "sidebar", {.width = Metric::sidebarWidth}));
    auto root = b.column(viewport, "asset-library-content", Metric::sidebarWidth,
                         Metric::libraryContentHeight, Metric::space4, Metric::space4);
    borders(root, false, true);
    good(viewport.set_layout_handler(
        [root](ii::gui::Rect bounds)
        {
            good(root.set_extent(
                {Metric::sidebarWidth,
                 std::max<double>(Metric::libraryContentHeight, double(bounds.height))}));
        }));
    const auto weak = weak_from_this();
    good(raw.retain(bindings_));
    auto title = b.row(root);
    b.glyph(title, "folder");
    b.label(title, "Asset Library", Metric::textDescription, {}, Metric::weightSemiBold);
    b.space(title);
    b.icon(title, "choose-library", "generalopen",
           [vm = vm_] { vm->send(congregation::ui::Action::choose_asset_library); });
    b.icon(title, "library-menu", "generalmoreVertical",
           [vm = vm_] { vm->send(congregation::ui::Action::open_asset_library); });
    auto source = take(b.controls.combo_box(
        root, "library-source",
        {.text = "Society · This Mac",
         .arrow = ii::gui::StepperArrow::down,
         .width = Metric::size216},
        {.clicked = [vm = vm_] { vm->send(congregation::ui::Action::choose_asset_library); }}));
    (void)source;
    auto route = b.row(root);
    b.glyph(route, "folder");
    location_ = b.label(route, "Storage / Asset Library", Metric::textBody, "asset-location");
    auto search =
        take(b.controls.input_field(root, "asset-search",
                                    {.placeholder = "Search assets",
                                     .width = Metric::size216,
                                     .accessibility_name = "Search Society assets",
                                     .style = ii::gui::InputStyle{.height = Metric::size22}},
                                    [weak](std::string text)
                                    {
                                        if (auto s = weak.lock())
                                        {
                                            s->filter_ = lower(std::move(text));
                                            s->refresh();
                                        }
                                    }));
    (void)search;
    auto tabs = take(b.controls.label_segmented_control(root, "asset-scope"));
    for (auto text : {"All", "Recent", "Saved"})
        b.button(
            tabs, "assets-" + lower(text), text,
            [weak, text]
            {
                if (auto s = weak.lock())
                {
                    s->scope_ = text;
                    s->refresh();
                }
            },
            0, ii::gui::ButtonTone::borderless);
    auto filters = b.row(root, {}, 0, Metric::size30);
    auto segments = take(b.controls.icon_segmented_control(filters, "asset-kinds"));
    for (const auto &[icon, category] :
         std::initializer_list<std::pair<const char *, const char *>>{
             {"folder", "All assets"},
             {"brush", "Brushes"},
             {"pattern-stamp", "Textures"},
             {"fileTypesimage", "References"},
             {"gradient", "Palettes"}})
        b.icon(segments, {}, icon,
               [weak, category]
               {
                   if (auto s = weak.lock())
                   {
                       s->category_ = category;
                       s->refresh();
                   }
               });
    b.space(filters);
    b.icon(filters, "library-options", "settings",
           [vm = vm_] { vm->send(congregation::ui::Action::choose_asset_library); });
    borders(filters, false, false, true);
    auto collections = b.column(root, "asset-collections");
    auto label = b.row(collections);
    b.glyph(label, "folder");
    b.label(label, "Collections", Metric::textDescription, {}, Metric::weightSemiBold);
    b.space(label);
    b.icon(label, {}, "group-layer");
    for (const auto &[text, icon] : std::initializer_list<std::pair<const char *, const char *>>{
             {"All assets", "folder"},
             {"References", "fileTypesimage"},
             {"Brushes", "brush"},
             {"Textures", "pattern-stamp"},
             {"Palettes", "gradient"},
             {"Guides", "perspective-grid"},
             {"Project assets", "folder"}})
    {
        auto row = b.row(collections);
        auto choice = b.choice(row, "collection-" + lower(text), text, icon,
                               [weak, text]
                               {
                                   if (auto s = weak.lock())
                                   {
                                       s->category_ = text;
                                       s->refresh();
                                   }
                               });
        good(choice.set_flex(1));
        collection_counts_[text] = b.label(row, "0", Metric::textCaption);
    }
    auto heading = b.row(root);
    count_ = b.label(heading, "8 assets", Metric::textDescription, {}, Metric::weightSemiBold);
    b.space(heading);
    take(b.controls.combo_box(heading, "asset-sort",
                              {.text = "Name ↑",
                               .tone = ii::gui::ButtonTone::borderless,
                               .arrow = ii::gui::StepperArrow::down,
                               .width = Metric::size97},
                              {.clicked = [weak]
                               {
                                   if (auto s = weak.lock())
                                   {
                                       std::ranges::reverse(s->entries_);
                                       s->refresh();
                                   }
                               }}));
    auto resultScroll = take(raw.scroll_view(
        root, "asset-scroll", {.width = Metric::size216, .height = Metric::size288}));
    results_ =
        take(raw.content_host(resultScroll, "asset-results", {Metric::size216, Metric::size288}));
    auto previewTitle = b.row(root);
    b.glyph(previewTitle, "fileTypesimage");
    b.label(previewTitle, "Preview", Metric::textDescription, {}, Metric::weightSemiBold);
    b.space(previewTitle);
    b.icon(previewTitle, {}, "generalopen",
           [vm = vm_] { vm->send(congregation::ui::Action::choose_asset_library); });
    borders(previewTitle, true);
    preview_card_ = take(lvrs::Card::create(raw, root, "asset-preview",
                                            {.type = lvrs::CardType::file_preview,
                                             .display_state = lvrs::CardDisplayState::selected,
                                             .selected = true,
                                             .width = Metric::size216,
                                             .height = Metric::size160}));
    preview_ = preview_card_.root();
    name_ = b.label(root, "", Metric::textCaption, "asset-selected-name");
    metadata_ = b.label(root, "Local copy · Available offline", Metric::textCaption);
    auto actions = b.row(root);
    b.button(
        actions, "insert-asset", "Insert as layer",
        [weak]
        {
            if (auto s = weak.lock())
                s->insert();
        },
        0, ii::gui::ButtonTone::borderless);
    b.space(actions);
    b.icon(actions, "asset-link", "link-layers",
           [weak]
           {
               if (auto s = weak.lock(); s && s->selected_ < s->entries_.size())
               {
                   const auto key = s->entries_[s->selected_].key;
                   if (s->saved_.contains(key))
                       s->saved_.erase(key);
                   else
                       s->saved_.insert(key);
                   s->vm_->status.set(s->saved_.contains(key)
                                          ? "Asset saved in this library session"
                                          : "Asset removed from saved items");
                   s->refresh();
               }
           });
    b.icon(actions, "open-society", "generalopen",
           [vm = vm_] { vm->send(congregation::ui::Action::open_asset_library); });
    b.space(root);
    auto status = b.row(root);
    b.label(status, "Society · Local library", Metric::textCaption);
    b.space(status);
    b.icon(status, "library-folder", "generalopen",
           [vm = vm_] { vm->send(congregation::ui::Action::choose_asset_library); });
    good(bindings_->bind(root, vm_->asset_root,
                         [weak](Widget, const std::string &path) -> ii::core::Result<void>
                         {
                             if (auto s = weak.lock())
                                 s->reload(path);
                             return {};
                         }));
    return root;
}
void AssetLibraryView::reload(std::string root)
{
    entries_.clear();
    selected_ = 0;
    widgets::good(location_.set_text(root.empty() ? "Storage / Asset Library" : root));
    if (root.empty())
        entries_ = {{"Coastal house.png", "PNG · 1536 × 1024", "fileTypesimage", "coast",
                     bundled_ / "coastal-house.png"},
                    {"Mountain study.png", "PNG · 1536 × 1024", "fileTypesimage", "mountain",
                     bundled_ / "mountain-study.png"},
                    {"Oil gestures", "Brush · 64 px", "brush", "oil", {}},
                    {"Linen grain.png", "Texture · 2048 × 2048", "pattern-stamp", "linen", {}},
                    {"Warm dusk", "Palette · 12 colors", "gradient", "palette", {}},
                    {"Perspective grid", "Guide · 2-point", "perspective-grid", "guide", {}},
                    {"Paper fibers.png", "Texture · 2048 × 2048", "pattern-stamp", "paper", {}},
                    {"Graphite pencils", "Brush · 24 px", "pencil", "graphite", {}}};
    else
    {
        std::error_code ec;
        std::filesystem::directory_iterator it(
            native_path(root), std::filesystem::directory_options::skip_permission_denied, ec),
            end;
        for (; !ec && it != end && entries_.size() < 512; it.increment(ec))
            if (it->is_regular_file(ec))
            {
                auto ext = lower(path_text(it->path().extension()));
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp")
                    entries_.push_back({path_text(it->path().filename()),
                                        "Image · Society local storage", "fileTypesimage",
                                        path_text(it->path()), it->path()});
            }
        std::ranges::sort(entries_, {}, &LibraryEntry::name);
    }
    refresh();
    if (!entries_.empty())
        select(0);
    else
    {
        widgets::good(name_.set_text("No supported images in this folder"));
        auto options = widgets::take(preview_card_.options());
        options.title = "No asset selected";
        options.metadata = "Choose a local library folder";
        options.preview_source = {};
        widgets::good(preview_card_.set_options(std::move(options)));
    }
}
ii::core::Result<void> AssetLibraryView::results(ii::gui::WidgetBuilder &raw)
{
    try
    {
        using namespace widgets;
        Builder b(raw, "asset-row/");
        auto root = b.column({}, "asset-list", 0, 0);
        entry_rows_.clear();
        const auto weak = weak_from_this();
        for (std::size_t i = 0; i < entries_.size(); ++i)
        {
            const auto entry = entries_[i];
            auto row = b.column(root, {}, 0, Metric::libraryRowHeight, Metric::space0);
            auto button = b.choice(row, "asset-" + std::to_string(i), entry.name, entry.icon,
                                   [weak, i]
                                   {
                                       if (auto s = weak.lock())
                                           s->select(i);
                                   });
            (void)button;
            auto metadata = b.row(row, {}, 0, Metric::size11, Metric::space0);
            auto indent = b.column(metadata, {}, Metric::size26, Metric::size11);
            (void)indent;
            b.label(metadata, entry.metadata, Metric::textCaption);
            entry_rows_.push_back(row);
        }
        return {};
    }
    catch (...)
    {
        return ii::core::fail(ii::core::ErrorCode::failed, "Cannot build asset results",
                              std::current_exception());
    }
}
void AssetLibraryView::refresh()
{
    const auto weak = weak_from_this();
    widgets::good(results_.set_content(
        [weak](ii::gui::WidgetBuilder &b) -> ii::core::Result<void>
        {
            if (auto s = weak.lock())
                return s->results(b);
            return {};
        }));
    std::size_t shown = 0;
    std::map<std::string, std::size_t> counts;
    const auto in_category = [&](const LibraryEntry &e, const std::string &category)
    {
        return category == "All assets" ||
               (category == "Project assets" && !vm_->asset_root.get().empty()) ||
               (category == "References" && !e.path.empty()) ||
               (category == "Brushes" && (e.key == "oil" || e.key == "graphite")) ||
               (category == "Textures" && (e.key == "linen" || e.key == "paper")) ||
               (category == "Palettes" && e.key == "palette") ||
               (category == "Guides" && e.key == "guide");
    };
    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        const auto &e = entries_[i];
        for (const auto &[category, widget] : collection_counts_)
            if (in_category(e, category))
                ++counts[category];
        const bool scope = scope_ == "All" || (scope_ == "Recent" && recent_.contains(e.key)) ||
                           (scope_ == "Saved" && saved_.contains(e.key));
        const bool visible =
            scope && in_category(e, category_) && lower(e.name).find(filter_) != std::string::npos;
        widgets::good(entry_rows_[i].set_visible(visible));
        shown += visible;
    }
    for (const auto &[category, widget] : collection_counts_)
        widgets::good(widget.set_text(std::to_string(counts[category])));

    widgets::good(results_.set_extent({Metric::size216, double(shown * Metric::libraryRowHeight)}));
    widgets::good(count_.set_text(std::to_string(shown) + " assets"));
}
void AssetLibraryView::select(std::size_t index)
{
    if (index >= entries_.size())
        return;
    selected_ = index;
    const auto &entry = entries_[index];
    recent_.insert(entry.key);
    widgets::good(
        name_.set_text("Asset Library / " + (entry.path.empty() ? entry.metadata : "References")));
    try
    {
        auto image = entry.path.empty()
                         ? widgets::take(lvrs::Icons::image(entry.icon, Metric::assetThumbnailSize))
                         : load_image(entry.path);
        auto options = widgets::take(preview_card_.options());
        options.title = entry.name;
        options.metadata = entry.metadata;
        options.preview_source = std::move(image);
        widgets::good(preview_card_.set_options(std::move(options)));
    }
    catch (const std::exception &e)
    {
        vm_->status.set(e.what());
    }
}
void AssetLibraryView::insert()
{
    if (selected_ >= entries_.size())
        return;
    const auto &e = entries_[selected_];
    if (!e.path.empty())
    {
        if (vm_->asset_inserted)
            vm_->asset_inserted(e.path);
    }
    else if (vm_->builtin_asset_inserted)
        vm_->builtin_asset_inserted(e.key);
}
} // namespace congregation::native
