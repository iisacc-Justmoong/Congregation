#include "DocumentFixture.h"
#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native; using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void send(CanvasView& view,ii::gui::WidgetInputKind kind,double x,double y){ii::gui::WidgetInput e;e.kind=kind;e.x=x;e.y=y;e.buttons=1;check(view.input(e).has_value(),"gradient input failed");}
unsigned alpha(const Artwork& art,int x,int y){auto r=renderFrameRegion(art.document(),0,{{x,y},{1,1}},{1,1});check(r.ok(),"gradient read failed");return r.pixels.pixels[0]>>24;}
int main() try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());check(dir.isValid(),"temp unavailable");
 Artwork art(dir.path());check(art.create(8,4),"gradient fixture failed");check(art.set_selection(SelectionGeometry::Rectangle,{2,0},{6,4}),"selection fixture failed");
 CanvasView view(art);view.tool="gradient";view.color=0xffff0000U;using K=ii::gui::WidgetInputKind;
 const auto before=art.revision();send(view,K::pointer_down,2,2);send(view,K::pointer_move,6,2);send(view,K::pointer_up,6,2);
 check(art.revision()>before && alpha(art,2,2)>alpha(art,5,2) && alpha(art,5,2)>0,"gradient did not follow drag");
 check(alpha(art,1,2)==0 && alpha(art,6,2)==0,"gradient escaped selection");
 check(art.undo() && alpha(art,2,2)==0 && art.selection_mask(),"gradient undo failed");
 check(art.redo() && alpha(art,2,2)>alpha(art,5,2),"gradient redo failed");
 check(art.undo(),"gradient undo before no-op failed");const auto revision=art.revision();
 send(view,K::pointer_down,3,2);send(view,K::pointer_up,3,2);check(art.revision()==revision && art.can_redo(),"zero-length gradient changed history");
 Artwork frames(dir.path());check(frames.create(8,4) && frames.timing(24,3),"gradient frame fixture failed");
 CanvasView frameView(frames);frameView.tool="gradient";frameView.color=0xffff0000U;frameView.set_frame(1);
 send(frameView,K::pointer_down,1,2);send(frameView,K::pointer_up,7,2);
 auto first=renderFrame(frames.document(),0),current=renderFrame(frames.document(),1),last=renderFrame(frames.document(),2);
 check(first.ok() && current.ok() && last.ok(),"gradient frame render failed");
 check((first.pixels.pixels[2*8+2]>>24)==0 && (current.pixels.pixels[2*8+2]>>24)>0 && (last.pixels.pixels[2*8+2]>>24)==0,"gradient escaped current frame");
 Artwork options(dir.path());check(options.create(4,1),"gradient options fixture failed");
 CanvasView configured(options);configured.tool="gradient";configured.gradient_kind=GradientKind::Radial;configured.gradient_spread=GradientSpread::Repeat;
 configured.gradient_stops={{0,0xffff0000U},{.5,0xff00ff00U},{1,0xff0000ffU}};configured.gradient_blend=RasterBlendMode::Multiply;
 send(configured,K::pointer_down,.5,.5);send(configured,K::pointer_up,2.5,.5);
 auto rendered=renderFrame(options.document(),0);check(rendered.ok(),"configured gradient render failed");
 check(rendered.pixels.pixels[0]==0xffff0000U && rendered.pixels.pixels[1]==0xff00ff00U && rendered.pixels.pixels[2]==0xffff0000U,"gradient settings not applied");
 check(layerProperties(options.document().layers.back()).blendMode==RasterBlendMode::Multiply,"gradient blend mode not applied");
 auto saved=options.snapshot();auto restored=decodeIisc({reinterpret_cast<const std::uint8_t*>(saved.data()),saved.size()});
 check(restored.ok() && layerProperties(restored.document.layers.back()).blendMode==RasterBlendMode::Multiply,"gradient blend mode did not survive serialization");
 auto savedPixels=renderFrame(restored.document,0);check(savedPixels.ok() && savedPixels.pixels.pixels==rendered.pixels.pixels,"gradient pixels changed after serialization");
 Document largeDoc;largeDoc.extent={9000,9000};largeDoc.assets.emplace_back(RasterAsset{"tiny",makeRasterLayer(1,1,0)});largeDoc.layers.emplace_back(StaticBitmapLayer{{"tiny-layer","Tiny"},StaticSource{"tiny"}});Artwork large(dir.path());check(large.replace(std::move(largeDoc))&&large.set_selection(SelectionGeometry::Rectangle,{100,100},{120,120}),"large gradient fixture failed");GradientDefinition gradient;gradient.start={100,110};gradient.end={120,110};gradient.stops={{0,0xffff0000U},{1,0xff0000ffU}};check(large.gradient(gradient),"large selected gradient failed");auto at=[&](const Artwork& a,int x,int y){auto result=renderFrameRegion(a.document(),0,{{x,y},{1,1}},{1,1});check(result.ok(),"large gradient sample failed");return result.pixels.pixels[0];};const auto left=at(large,104,110),right=at(large,115,110);check((left>>24)==255&&(right>>24)==255&&((left>>16)&255)>((right>>16)&255)&&at(large,99,110)==0,"large gradient selection or interpolation failed");check(large.undo()&&at(large,104,110)==0&&large.has_selection()&&large.can_redo(),"large gradient undo failed");auto transparent=gradient;transparent.stops={{0,0},{1,0}};const auto noOpRevision=large.revision();check(large.gradient(transparent)&&large.revision()==noOpRevision&&large.can_redo(),"transparent gradient lost redo");check(large.redo()&&at(large,104,110)==left,"large gradient redo failed");const auto gradientPath=dir.path()/"selected-gradient.iisc";check(large.save(gradientPath),"large gradient save failed");Artwork reopenedGradient(dir.path());check(open_document_fixture(reopenedGradient, gradientPath)&&at(reopenedGradient,104,110)==left&&!reopenedGradient.has_selection(),"large gradient reopen failed");
 Document oversizedDoc;oversizedDoc.extent={9000,9000};oversizedDoc.assets.emplace_back(RasterAsset{"oversized",makeRasterLayer(1,1,0)});oversizedDoc.layers.emplace_back(StaticBitmapLayer{{"oversized-layer","Oversized"},StaticSource{"oversized"}});Artwork oversized(dir.path());check(oversized.replace(std::move(oversizedDoc))&&oversized.set_selection(SelectionGeometry::Rectangle,{0,0},{9000,9000})&&oversized.set_selection(SelectionGeometry::Rectangle,{0,0},{10,10})&&oversized.undo()&&oversized.can_redo(),"oversized gradient fixture failed");const auto oversizedRevision=oversized.revision();check(!oversized.gradient(gradient)&&oversized.revision()==oversizedRevision&&oversized.can_redo(),"oversized gradient changed document or redo");
 Document ellipseDoc;ellipseDoc.extent={9000,9000};ellipseDoc.timeline={{24,1},3};ellipseDoc.assets.emplace_back(RasterAsset{"ellipse-base",makeRasterLayer(1,1,0)});ellipseDoc.layers.emplace_back(StaticBitmapLayer{{"ellipse-base-layer","Base"},StaticSource{"ellipse-base"}});Artwork ellipseGradient(dir.path());check(ellipseGradient.replace(std::move(ellipseDoc))&&ellipseGradient.set_selection(SelectionGeometry::Ellipse,{100,100},{120,120}),"ellipse gradient fixture failed");check(ellipseGradient.gradient(gradient,1),"ellipse frame gradient failed");auto framePixel=[&](int frame,int x,int y){auto result=renderFrameRegion(ellipseGradient.document(),frame,{{x,y},{1,1}},{1,1});check(result.ok(),"ellipse gradient frame sample failed");return result.pixels.pixels[0];};check((framePixel(1,110,110)>>24)!=0&&framePixel(1,100,100)==0&&framePixel(0,110,110)==0&&framePixel(2,110,110)==0,"ellipse gradient escaped selection or frame");check(ellipseGradient.undo()&&framePixel(1,110,110)==0&&ellipseGradient.redo()&&(framePixel(1,110,110)>>24)!=0,"ellipse frame gradient history failed");
 Document infiniteDoc;infiniteDoc.canvasMode=CanvasMode::Infinite;infiniteDoc.infiniteCanvas.origin={-64,-64};infiniteDoc.extent={128,128};infiniteDoc.assets.emplace_back(ChunkedRasterAsset{"gradient-chunks",{}});infiniteDoc.layers.emplace_back(StaticBitmapLayer{{"gradient-base","Base"},StaticSource{"gradient-chunks"}});Artwork infiniteGradient(dir.path());check(infiniteGradient.replace(std::move(infiniteDoc))&&infiniteGradient.set_selection(SelectionGeometry::Rectangle,{-20,-10},{20,10}),"infinite gradient fixture failed");gradient.start={-20,0};gradient.end={20,0};check(infiniteGradient.gradient(gradient)&&at(infiniteGradient,-10,0)!=0&&at(infiniteGradient,-30,0)==0,"infinite selected gradient failed");check(infiniteGradient.undo()&&at(infiniteGradient,-10,0)==0&&infiniteGradient.redo()&&at(infiniteGradient,-10,0)!=0,"infinite gradient history failed");
 std::cout<<"GRADIENT_INTEGRATION_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
