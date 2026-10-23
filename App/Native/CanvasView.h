#pragma once
#include "Artwork.h"
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Image.h>
#include <map>
#include <tuple>

namespace congregation::native
{
struct Viewport final
{
    double scale = 1, x = 0, y = 0, width = 1, height = 1;
    std::size_t visible_tiles = 0;
    std::uint64_t uploads = 0;
};
class CanvasView final
{
  public:
    explicit CanvasView(Artwork& artwork) : artwork_(artwork) {}
    void attach(ii::gui::Widget);
    void resize(ii::gui::Rect);
    void refresh(bool document_changed = true);
    void render();
    void fit();
    void zoom(double factor, double x = -1, double y = -1);
    ii::core::Result<bool> input(const ii::gui::WidgetInput&);
    const Viewport& viewport() const { return viewport_; }
    std::string tool = "brush", text = "Text";
    std::uint32_t color = 0xff1a1a1a;
    double brush_size = 2, opacity = 1;
    bool presentation = false;
    iiSharedCanvas::BitmapBrush settings;
    std::function<void(std::string)> status;
    std::function<void(std::uint32_t)> color_picked;

  private:
    iiSharedCanvas::Point world(double x, double y) const;
    bool hit(iiSharedCanvas::Point);
    void cancel();
    Artwork& artwork_;
    ii::gui::Widget widget_;
    Viewport viewport_;
    std::map<std::tuple<int, int, int>, std::shared_ptr<const ii::gui::RasterImage>> cache_;
    bool fitted_ = true, attached_ = false, drag_ = false, panning_ = false, zooming_ = false, space_ = false,
         selecting_ = false;
    iiSharedCanvas::Point down_{}, last_{};
};
} // namespace congregation::native
