#include "EditorLayout.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace congregation::native::editor
{
namespace
{
void good(ii::core::Result<void> value)
{
    if (!value) throw std::runtime_error(value.error().message);
}
void fill(ii::gui::Widget widget, ii::gui::Rect bounds, ii::gui::WidgetColor color)
{
    ii::gui::WidgetDrawing drawing;
    if(bounds.width>0&&bounds.height>0) drawing.panels.push_back({{0,0,bounds.width,bounds.height},linear(color),linear(color),0});
    good(widget.set_drawing(std::move(drawing)));
}
}
Fit fit(double width, double height, double document_width, double document_height)
{
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(document_width) ||
        !std::isfinite(document_height) || width<=0 || height<=0 || document_width<=0 || document_height<=0)
        return {1,0,0};
    const double horizontal=std::max(24.,std::round(width*.09));
    const double top=std::max(24.,std::round(height*.12));
    const double bottom=std::max(24.,std::round(height*.10));
    const double scale=std::max(1./65536,std::min({1.,std::max(1.,width-2*horizontal)/document_width,
                                                    std::max(1.,height-top-bottom)/document_height}));
    return {scale,(width-document_width*scale)/2,(height-document_height*scale)/2};
}
ii::gui::Color linear(ii::gui::WidgetColor c)
{
    const auto channel=[](double v){return float(v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4));};
    return {channel(c.red),channel(c.green),channel(c.blue),float(c.alpha)};
}
void panel(ii::gui::Widget widget, ii::gui::WidgetColor color)
{
    if (auto s=widget.snapshot()) fill(widget,s->bounds,color);
    good(widget.set_layout_handler([widget,color](ii::gui::Rect r){fill(widget,r,color);}));
}
void brush_preview(ii::gui::Widget widget, double size, ii::gui::WidgetColor color)
{
    ii::gui::WidgetDrawing drawing;
    const auto diameter=5+std::clamp((size-1)/47,0.,1.)*16;
    drawing.panels.push_back({{0,0,28,28},linear({1,1,1,.04}),linear({1,1,1,.15}),14,1});
    drawing.panels.push_back({{float((28-diameter)/2),float((28-diameter)/2),float(diameter),float(diameter)},linear(color),linear(color),float(diameter/2)});
    good(widget.set_drawing(std::move(drawing)));
}
double pressure_value(double y) { return std::clamp(1-(y-6)/80,0.,1.); }
int pressure_point(double x,double width) { return x<width/4?0:x>width*3/4?2:1; }
void pressure_curve(ii::gui::Widget widget,double minimum,double center,double maximum)
{
    const auto snapshot=widget.snapshot();if(!snapshot)return;
    const double width=snapshot->bounds.width>12?snapshot->bounds.width:316,left=6,right=width-6;
    const double values[]{minimum,center,maximum};
    const auto primary=linear({135./255,74./255,1,1});
    ii::gui::WidgetDrawing drawing;
    for(int i=0;i<5;++i)drawing.panels.push_back({{6,float(6+i*20),float(width-12),1},linear({1,1,1,.12}),linear({1,1,1,.12})});
    for(int i=0;i<=int(right-left);++i){
        const double position=i/(right-left),segment=position<.5?0:1,t=position*2-segment;
        const double value=values[int(segment)]*(1-t)+values[int(segment)+1]*t;
        drawing.panels.push_back({{float(left+i-1.5),float(6+(1-value)*80-1.5),3,3},primary,primary,1.5});
    }
    for(int i=0;i<3;++i){
        const auto x=left+i*(right-left)/2,y=6+(1-values[i])*80;
        drawing.panels.push_back({{float(x-6),float(y-6),12,12},linear({.94,.95,.97,1}),linear({.94,.95,.97,1}),6});
        drawing.panels.push_back({{float(x-6),float(y-6),12,12},primary,primary,6,2});
    }
    good(widget.set_drawing(std::move(drawing)));
}
void tool_button(ii::gui::Widget widget, bool selected)
{
    good(widget.set_button_style({.fill=selected?ii::gui::WidgetColor{.19,.20,.20,1}:ii::gui::WidgetColor{0,0,0,0},
        .hover_fill=ii::gui::WidgetColor{.23,.24,.24,1},.pressed_fill=ii::gui::WidgetColor{.13,.14,.14,1}}));
}
} // namespace congregation::native::editor
