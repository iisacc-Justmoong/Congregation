#pragma once

#include <iiSDKCore/Runtime/Query.h>

#include <cstdint>
#include <string>
#include <thread>
#include <variant>

namespace congregation
{
struct Color final
{
    double red = 0;
    double green = 0;
    double blue = 0;
    double alpha = 1;
    bool valid = true;

    static constexpr Color fromRgb(std::uint32_t rgb)
    {
        return {double((rgb >> 16) & 255) / 255, double((rgb >> 8) & 255) / 255,
                double(rgb & 255) / 255, 1};
    }
    bool operator==(const Color&) const = default;
};

struct CanvasSnapshot final
{
    Color brushColor = Color::fromRgb(0x1a1a1a);
    double brushSize = 2;
    double brushFlow = 1;
    double brushOpacity = 1;
    double brushHardness = 1;
    double brushSpacing = 0;
    double brushSpacingRatio = 0;
    double pressureMinimum = 0;
    double pressureCenter = 0.5;
    double pressureMaximum = 1;
    bool pressureControlsOpacity = true;
    double stabilizerStrength = 0;
    std::string tool = "brush";
    std::string shape = "rectangle";
    int canvasWidth = 1;
    int canvasHeight = 1;
    // Editing controls have their own revision; this is not a saved-document revision.
    std::uint64_t revision = 0;
    bool operator==(const CanvasSnapshot&) const = default;
};

enum class RealProperty
{
    brushSize,
    brushFlow,
    brushOpacity,
    brushHardness,
    brushSpacing,
    brushSpacingRatio,
    pressureMinimum,
    pressureCenter,
    pressureMaximum,
    stabilizerStrength
};
enum class Dimension
{
    width,
    height
};
struct ReadCanvas final
{
};
struct SetReal final
{
    RealProperty property;
    double value;
};
struct SetColor final
{
    Color value;
};
struct SetTool final
{
    std::string value;
};
struct SetShape final
{
    std::string value;
};
struct SetPressureOpacity final
{
    bool value;
};
struct SetDimension final
{
    Dimension dimension;
    int value;
};
using CanvasQuery = std::variant<ReadCanvas, SetReal, SetColor, SetTool, SetShape,
                                 SetPressureOpacity, SetDimension>;

// The host owns this foreground editing object. Queries can be posted to its
// owner EventLoop; synchronous GUI property updates stay on that same thread.
class CanvasDocument final
{
  public:
    CanvasDocument();
    [[nodiscard]] CanvasSnapshot snapshot() const;
    [[nodiscard]] ii::runtime::QueryPort<CanvasQuery, CanvasSnapshot>& queries() noexcept;

  private:
    void checkOwner() const;
    CanvasSnapshot apply(const CanvasQuery& query);
    void setReal(const SetReal& query);

    std::thread::id m_owner;
    CanvasSnapshot m_state;
    ii::runtime::QueryPort<CanvasQuery, CanvasSnapshot> m_queries;
};
} // namespace congregation
