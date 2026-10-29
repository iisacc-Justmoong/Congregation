#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace iiSharedCanvas;
using namespace congregation::native;
namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
unsigned alpha(const Document &doc, int frame, int x, int y) {
    auto value = renderFrameRegion(doc, frame, {{x,y}, {1,1}}, {1,1});
    check(value.ok() && value.pixels.pixels.size() == 1, "render sample failed");
    return value.pixels.pixels[0] >> 24;
}
Document document(bool seeded) {
    Document d; d.extent={160,80}; d.timeline={{24,1},10};
    d.artboards.push_back({"board", "Board", {{40,0}, {100,80}}, 0, true});
    d.assets.emplace_back(RasterAsset{"other-asset",makeRasterLayer(100,80,0)});
    auto pixels = makeRasterLayer(100,80,0);
    if (seeded) pixels.pixels[40*100+10]=0xffff0000U;
    d.assets.emplace_back(RasterAsset{"paint",std::move(pixels)});
    d.layers.emplace_back(StaticBitmapLayer{{"other","Other"},StaticSource{"other-asset"}});
    StaticBitmapLayer layer{{"layer","Paint"},StaticSource{"paint"}};
    layer.properties.artboardId="board";
    layer.properties.transform.translationX=3;
    layer.properties.motion={{0,MotionValue{.position={0,0}}},{4,MotionValue{.position={20,0}}}};
    d.layers.emplace_back(layer);
    return d;
}
void send(CanvasView &view, ii::gui::WidgetInputKind kind, double x, double y) {
    ii::gui::WidgetInput event; event.kind=kind; event.x=x; event.y=y; event.buttons=1;
    auto result=view.input(event);
    check(result.has_value(), "view input returned an error");
}
}
int main(int argc,char **argv) {
    try {
        check(argc==2,"expected test mode");
        const std::string_view mode(argv[1]);
        const std::filesystem::path parent = std::filesystem::temp_directory_path();
        std::filesystem::create_directories(parent);
        iiFileProvider::native::TemporaryDirectory temp(parent);
        check(temp.isValid(),"temporary directory failed");
        Artwork art(temp.path());
        check(art.replace(document(mode=="eyedropper" || mode=="selection" || mode=="fill")),"document creation failed");
        CanvasView view(art); view.brush_size=8; view.color=0xffff0000U; view.set_frame(4);
        using K=ii::gui::WidgetInputKind;
        if (mode=="paint") {
            send(view,K::pointer_down,73,40); send(view,K::pointer_up,73,40);
            const auto intended=alpha(art.document(),4,73,40), displaced=alpha(art.document(),4,93,40);
            std::cout << "view_intended_alpha=" << intended << " displaced_alpha=" << displaced << '\n';
            check(intended>0 && displaced==0,"view did not pass the displayed frame to painting");
        } else if (mode=="eyedropper") {
            check(alpha(art.document(),4,73,40)>0 && alpha(art.document(),0,73,40)==0,"eyedropper control");
            std::uint32_t picked=0; view.color_picked=[&](std::uint32_t color){picked=color;};
            view.tool="eyedropper"; send(view,K::pointer_down,73,40);
            std::cout << "picked=" << picked << '\n';
            check(picked==0xffff0000U,"eyedropper did not sample the displayed frame");
        } else if (mode=="selection") {
            check(art.select("other"),"cannot select control layer");
            view.tool="select"; send(view,K::pointer_down,73,40);
            std::cout << "selected=" << art.selected() << '\n';
            check(art.selected()=="layer","hit test did not use the displayed frame");
        } else if (mode=="fill") {
            check(alpha(art.document(),4,73,40)>0 && alpha(art.document(),0,73,40)==0,"fill frame control");
            view.tool="fill"; view.color=0xff00ff00U; send(view,K::pointer_down,73,40);
            auto painted=renderFrameRegion(art.document(),4,{{73,40},{1,1}},{1,1});
            auto distant=renderFrameRegion(art.document(),4,{{5,5},{1,1}},{1,1});
            check(painted.ok() && painted.pixels.pixels[0]==0xff00ff00U,"fill did not color the displayed frame target");
            check(distant.ok() && (distant.pixels.pixels[0]>>24)==0,"fill sampled an unrelated frame region");
        } else if (mode=="cancel") {
            const auto revision=art.revision();
            send(view,K::pointer_down,73,40);
            check(art.stroke_active(),"stroke did not start");
            view.set_frame(0);
            check(!art.stroke_active() && art.revision()==revision,"frame change did not cancel the active stroke");
            send(view,K::pointer_up,73,40);
            check(art.revision()==revision,"release committed a cancelled stroke");
        } else throw std::runtime_error("unknown test mode");
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
