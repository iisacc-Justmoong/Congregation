#include "CanvasDocument.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace congregation
{
namespace
{
bool equivalent(double left, double right)
{
    return left == right ||
           std::abs(left - right) * 1e12 <= std::min(std::abs(left), std::abs(right));
}

void assign(double& target, double value)
{
    if (!equivalent(target, value))
    {
        target = value;
    }
}

std::string normalizeShape(std::string value)
{
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos)
    {
        return "rectangle";
    }
    value = value.substr(begin, value.find_last_not_of(whitespace) - begin + 1);
    for (char& character : value)
    {
        if (character >= 'A' && character <= 'Z')
        {
            character += 'a' - 'A';
        }
    }
    if (value == "triagle")
    {
        value = "triangle";
    }
    constexpr std::array<std::string_view, 7> shapes{
        "rectangle", "ellipse", "triangle", "diamond", "star", "rectanglebubble", "ellipsebubble"};
    return std::ranges::find(shapes, value) == shapes.end() ? "rectangle" : value;
}
} // namespace

CanvasDocument::CanvasDocument()
    : m_owner(std::this_thread::get_id()),
      m_queries([this](const CanvasQuery& query) { return apply(query); })
{
}

void CanvasDocument::checkOwner() const
{
    if (std::this_thread::get_id() != m_owner)
    {
        throw std::logic_error("CanvasDocument requires its owning event thread");
    }
}

CanvasSnapshot CanvasDocument::snapshot() const
{
    checkOwner();
    return m_state;
}

ii::runtime::QueryPort<CanvasQuery, CanvasSnapshot>& CanvasDocument::queries() noexcept
{
    return m_queries;
}

CanvasSnapshot CanvasDocument::apply(const CanvasQuery& query)
{
    checkOwner();
    const CanvasSnapshot previous = m_state;
    std::visit(
        [this](const auto& command)
        {
            using Command = std::decay_t<decltype(command)>;
            if constexpr (std::is_same_v<Command, SetReal>)
            {
                setReal(command);
            }
            else if constexpr (std::is_same_v<Command, SetColor>)
            {
                const auto& color = command.value;
                if (std::isfinite(color.red) && std::isfinite(color.green) &&
                    std::isfinite(color.blue) && std::isfinite(color.alpha))
                {
                    m_state.brushColor = color;
                }
            }
            else if constexpr (std::is_same_v<Command, SetTool>)
            {
                constexpr std::array<std::string_view, 8> tools{"brush", "eraser", "pan",  "move",
                                                                "zoom",  "shape",  "fill", "text"};
                m_state.tool = std::ranges::find(tools, command.value) == tools.end()
                                   ? "brush"
                                   : command.value;
            }
            else if constexpr (std::is_same_v<Command, SetShape>)
            {
                m_state.shape = normalizeShape(command.value);
            }
            else if constexpr (std::is_same_v<Command, SetPressureOpacity>)
            {
                m_state.pressureControlsOpacity = command.value;
            }
            else if constexpr (std::is_same_v<Command, SetDimension>)
            {
                (command.dimension == Dimension::width ? m_state.canvasWidth
                                                       : m_state.canvasHeight) =
                    std::max(1, command.value);
            }
        },
        query);
    if (previous != m_state)
    {
        ++m_state.revision;
    }
    return m_state;
}

void CanvasDocument::setReal(const SetReal& query)
{
    if (!std::isfinite(query.value))
    {
        return;
    }
    const double unit = std::clamp(query.value, 0.0, 1.0);
    switch (query.property)
    {
    case RealProperty::brushSize:
        assign(m_state.brushSize, std::clamp(query.value, 1.0, 48.0));
        break;
    case RealProperty::brushFlow:
        assign(m_state.brushFlow, unit);
        break;
    case RealProperty::brushOpacity:
        assign(m_state.brushOpacity, unit);
        break;
    case RealProperty::brushHardness:
        assign(m_state.brushHardness, std::clamp(query.value, 0.01, 1.0));
        break;
    case RealProperty::brushSpacing:
        assign(m_state.brushSpacing, std::max(0.0, query.value));
        break;
    case RealProperty::brushSpacingRatio:
        assign(m_state.brushSpacingRatio, unit);
        break;
    case RealProperty::stabilizerStrength:
        assign(m_state.stabilizerStrength, unit);
        break;
    case RealProperty::pressureMinimum:
        assign(m_state.pressureMinimum, unit);
        assign(m_state.pressureCenter, std::max(m_state.pressureCenter, m_state.pressureMinimum));
        assign(m_state.pressureMaximum, std::max(m_state.pressureMaximum, m_state.pressureMinimum));
        break;
    case RealProperty::pressureCenter:
        assign(m_state.pressureCenter,
               std::clamp(query.value, m_state.pressureMinimum, m_state.pressureMaximum));
        break;
    case RealProperty::pressureMaximum:
        assign(m_state.pressureMaximum, unit);
        assign(m_state.pressureMinimum, std::min(m_state.pressureMinimum, m_state.pressureMaximum));
        assign(m_state.pressureCenter, std::clamp(m_state.pressureCenter, m_state.pressureMinimum,
                                                  m_state.pressureMaximum));
        break;
    }
}
} // namespace congregation
