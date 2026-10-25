#include <Native/ToolSettings.h>
#include <Native/TimelineModel.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace congregation::native;
void require(bool b, const char* m)
{
    if (!b)
        throw std::runtime_error(m);
}
int main()
{
    try
    {
        ToolSettings settings;
        require(ToolSettings::specs().size() == 22,
                "The Figma Tool panel must preserve all 22 parameters");
        require(settings.value(ToolParameter::size) == 64 &&
                    settings.label(ToolParameter::hardness) == "85%",
                "Figma parameter defaults changed");
        require(std::abs(settings.normalized(ToolParameter::size) - .6) < 1e-10,
                "Brush size must use a logarithmic slider");
        require(settings.set_normalized(ToolParameter::size, .7) &&
                    settings.value(ToolParameter::size) == 128,
                "Dragging size must update the physical value");
        settings.step(ToolParameter::size, -1);
        require(settings.value(ToolParameter::size) == 127,
                "Step buttons must adjust one physical unit");
        settings.set(ToolParameter::count, 1000);
        require(settings.value(ToolParameter::count) == 64, "Discrete parameter upper bound lost");
        settings.set(ToolParameter::count, -5);
        require(settings.value(ToolParameter::count) == 1, "Discrete parameter lower bound lost");
        auto before = settings.serialize();
        require(!settings.set(ToolParameter::opacity, std::numeric_limits<double>::quiet_NaN()) &&
                    settings.serialize() == before,
                "Invalid input must preserve settings");
        ToolSettings restored;
        require(restored.restore(before) && restored.serialize() == before,
                "Tool settings must round trip without a document");
        require(!restored.restore("congregation.tools=1\nsize=12\n") &&
                    restored.serialize() == before,
                "Partial or malformed preferences must be atomic");
        auto brush = settings.brush();
        require(std::abs(brush.hardness - .85) < 1e-10 && std::abs(brush.flow - .9) < 1e-10,
                "Controls are disconnected from the paint engine");
        settings.set(ToolParameter::angle, 90);
        settings.set(ToolParameter::roundness, 50);
        brush = settings.brush();
        require(std::abs(brush.engineState->rasterizer.shape.angleRadians - std::acos(-1.) / 2) <
                        1e-10 &&
                    brush.engineState->rasterizer.shape.roundness == .5,
                "Tip shape parameters do not reach iiPaintEngine");
        TimelineModel timeline;
        require(timeline.time_text() == "00:01.000", "Timeline must display elapsed time");
        timeline.scrub(.251);
        require(timeline.frame() == 6, "Timeline dragging must snap to frame ticks");
        timeline.scrub(100);
        require(timeline.frame() == 48, "Timeline end clamp lost");
        timeline.scrub(-1);
        require(timeline.frame() == 0, "Timeline start clamp lost");
        timeline.set_rate(30);
        timeline.scrub(1);
        require(timeline.frame() == 30 && timeline.time_text() == "00:01.000",
                "FPS changes must preserve time units");
        require(TimelineModel::format_time(3661.125) == "61:01.125",
                "Long timeline formatting lost minute precision");
        std::cout << "TOOL_SETTINGS_OK parameters=22 log_size=true limits=true persistent=true "
                     "engine=true time_ticks=true\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
