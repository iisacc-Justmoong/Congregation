#include <Native/StartupCanvas.h>
#include <limits>
#include <stdexcept>

using namespace congregation::native;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    auto wide=startup_canvas_definition({3840,2160});
    check(wide && wide->pixel_width==3840 && wide->pixel_height==2160 &&
          wide->width==3840 && wide->height==2160 && wide->background=="Transparent",
          "display-sized landscape canvas is incorrect");
    auto portrait=startup_canvas_definition({2064,2752});
    check(portrait && portrait->pixel_width==2064 && portrait->pixel_height==2752,
          "display-sized portrait canvas is incorrect");
    check(!startup_canvas_definition({0,1080}),"missing display was accepted");
    check(!startup_canvas_definition({-1,1080}),"negative display was accepted");
    check(!startup_canvas_definition({32769,1}),"over-limit canvas was accepted");
    check(!startup_canvas_definition({32768,32768}),"over-budget canvas was accepted");
}
