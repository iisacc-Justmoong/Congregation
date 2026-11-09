#include <Native/Artwork.h>
#include <Native/CanvasView.h>
#include <iiFileProviderNative.h>
#include <iiSharedCanvasNative.h>
#include <Brush/BrushResolve.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace congregation::native;
using namespace iiSharedCanvas;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main() try {
 iiFileProvider::native::TemporaryDirectory dir(std::filesystem::temp_directory_path());
 check(dir.isValid(),"temporary directory unavailable");
 using K=ii::gui::WidgetInputKind;
 // The native event path must carry pen tilt and measured time to brush pixels.
 auto penStamp=[&](double tilt, double duration, bool airbrush) {
   Artwork penArt(dir.path());check(penArt.create(64,64),"pen input fixture failed");
   CanvasView penView(penArt);penView.brush_size=16;penView.color=0xff3366ccU;
   BrushPreset preset;preset.size=16;auto resolved=resolveBrushPreset(preset,penView.color);check(resolved.brush.has_value(),"brush fixture failed");penView.settings.engineState=resolved.brush;
   if (airbrush) {penView.settings.flow=.1;penView.settings.engineState->rasterizer.airbrushEnabled=true;penView.settings.engineState->rasterizer.airbrushRate=10;}
   else penView.settings.engineState->dynamics.tiltToEllipse=1;
   ii::gui::WidgetInput event;event.x=32;event.y=32;event.buttons=1;event.device=ii::gui::PointerDevice::pen;event.pressure=1;event.tilt_x=tilt;event.timestamp=100;
   std::string error;penView.status=[&](std::string value){error=std::move(value);};event.kind=K::pointer_down;check(penView.input(event).has_value() && penArt.stroke_active(),error.empty()?"pen input begin failed":error.c_str());
   event.kind=K::pointer_up;event.timestamp+=duration;check(penView.input(event).has_value(),"pen input end failed");
   auto rendered=renderFrame(penArt.document(),0);check(rendered.ok(),"pen input render failed");return rendered.pixels;
 };
 const auto upright=penStamp(0,.01,false),tilted=penStamp(72,.01,false);
 auto footprint=[](const RasterLayer& raster) {int l=raster.width,r=-1,t=raster.height,b=-1;for(int y=0;y<raster.height;++y)for(int x=0;x<raster.width;++x)if(raster.pixels[std::size_t(y)*raster.width+x]>>24){l=std::min(l,x);r=std::max(r,x);t=std::min(t,y);b=std::max(b,y);}return std::pair{r-l+1,b-t+1};};
 const auto up=footprint(upright),tilt=footprint(tilted);
 std::cout << "upright=" << up.first << "x" << up.second << " tilted=" << tilt.first << "x" << tilt.second << "\n";
 check(tilt.first*up.second>up.first*tilt.second && tilt.second<up.second,"pen tilt did not reach brush engine");
 const auto shortSpray=penStamp(0,.01,true),longSpray=penStamp(0,1,true);
 check((longSpray.pixels[32*64+32]>>24)>(shortSpray.pixels[32*64+32]>>24),"pen elapsed time did not reach brush engine");
 Artwork mixedArt(dir.path());check(mixedArt.create(32,32),"mixed input fixture failed");
 BitmapBrush mixedBrush;mixedBrush.size=4;mixedBrush.argb=0xff000000U;
 StrokePoint sample;sample.position={10,10};sample.time=20;
 check(mixedArt.begin_input(sample,mixedBrush),"mixed input begin failed");
 sample.position={12,10};sample.time=20.2;check(mixedArt.extend_input(sample),"mixed input extend failed");
 check(mixedArt.end({14,10},1),"legacy end after measured sample failed");
 check(mixedArt.undo() && mixedArt.redo(),"mixed input history failed");
 std::cout << "PEN_INPUT_INTEGRATION_OK\n";return 0;
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
