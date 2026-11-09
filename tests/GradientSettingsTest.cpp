#include <Native/GradientSettings.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace congregation::native;using namespace iiSharedCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main() try {
 GradientSettings preserve;check(preserve.set_color(0xffff0000U)&&preserve.select(1)&&preserve.set_color(0x400000ffU),"gradient color fixture");
 GradientDefinition reference;reference.start={0,0};reference.end={1,0};reference.stops=preserve.stops();
 auto original=gradientRaster({1,1},reference);check(original.ok(),"gradient reference raster failed");
 check(preserve.add(),"add gradient midpoint failed");reference.stops=preserve.stops();auto updated=gradientRaster({1,1},reference);
 check(updated.ok()&&updated.pixels.pixels[0]==original.pixels.pixels[0],"adding a stop changed the gradient midpoint");
 GradientSettings g;check(g.stops().size()==2 && g.selected()==0,"default gradient stops");
 g.set_default_start(0xff123456U);check(g.stops()[0].argb==0xff123456U,"default foreground color");
 check(g.add() && g.stops().size()==3 && g.selected()==1 && g.stops()[1].position==.5,"add midpoint stop");
 check(g.set_position(.25) && g.stops()[1].position==.25,"stop position");
 check(g.set_color(0xff336699U) && g.set_alpha(.5) && g.stops()[1].argb==0x80336699U,"stop color and alpha");
 check(!g.set_position(std::numeric_limits<double>::quiet_NaN()),"invalid stop position accepted");
 check(g.select(0) && !g.set_position(.25) && !g.remove(),"endpoint edited or removed");
 check(g.set_color(0xffabcdefU),"start custom color");g.set_default_start(0xff112233U);check(g.stops()[0].argb==0xffabcdefU,"custom start overwritten");
 check(g.select(1) && g.remove() && g.stops().size()==2,"remove middle stop");
 g.set_radial(true);g.set_repeat(true);g.cycle_blend();check(g.kind()==GradientKind::Radial && g.spread()==GradientSpread::Repeat && g.blend()==RasterBlendMode::Multiply,"gradient mode settings");
 check(g.revision.get()>0,"settings did not publish changes");
 while(g.stops().size()<256)check(g.add(),"failed to add bounded stops");
 check(!g.add() && g.stops().size()==256,"stop limit not enforced");
 for(std::size_t i=1;i<g.stops().size();++i)check(g.stops()[i-1].position<g.stops()[i].position,"stops not ordered");
 std::cout<<"GRADIENT_SETTINGS_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
