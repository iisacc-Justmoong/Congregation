#include "TimelineModel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace congregation::native
{
std::string TimelineModel::format_time(double value)
{
    if (!std::isfinite(value))
        value = 0;
    auto ms = std::llround(std::clamp(value, 0., 5999999.) * 1000);
    char text[40];
    std::snprintf(text, sizeof(text), "%02lld:%02lld.%03lld", ms / 60000, ms / 1000 % 60,
                  ms % 1000);
    return text;
}
bool TimelineModel::scrub(double seconds)
{
    if (!std::isfinite(seconds))
        return false;
    auto next = state.get();
    const auto f = int(std::llround(std::clamp(seconds, 0., next.duration) * next.rate));
    if (f == next.frame)
        return false;
    next.frame = f;
    state.set(next);
    return true;
}
void TimelineModel::set_rate(int rate)
{
    auto next = state.get();
    const auto time = seconds();
    next.rate = std::clamp(rate, 1, 240);
    next.frame = int(std::llround(time * next.rate));
    state.set(next);
}
void TimelineModel::set_duration(double duration)
{
    if (!std::isfinite(duration))
        return;
    auto next = state.get();
    next.duration = std::clamp(duration, 1. / next.rate, 86400.);
    next.frame = std::min(next.frame, int(std::llround(next.duration * next.rate)));
    state.set(next);
}
void TimelineModel::playing(bool play)
{
    auto next = state.get();
    next.playing = play;
    state.set(next);
}
void TimelineModel::advance(int count)
{
    auto next = state.get();
    const auto end = int(std::llround(next.duration * next.rate));
    next.frame = (next.frame + count) % (end + 1);
    if (next.frame < 0)
        next.frame += end + 1;
    state.set(next);
}
} // namespace congregation::native
