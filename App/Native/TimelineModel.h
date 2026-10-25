#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
namespace congregation::native
{
struct TimelinePosition final
{
    int frame = 24, rate = 24;
    double duration = 2;
    bool playing = false;
    friend bool operator==(const TimelinePosition&, const TimelinePosition&) = default;
};
class TimelineModel final
{
  public:
    [[nodiscard]] int frame() const { return state.get().frame; }
    [[nodiscard]] double seconds() const { return double(frame()) / state.get().rate; }
    [[nodiscard]] std::string time_text() const { return format_time(seconds()); }
    [[nodiscard]] static std::string format_time(double);
    bool scrub(double seconds);
    void set_rate(int);
    void set_duration(double);
    void playing(bool);
    void advance(int frames = 1);
    ii::core::Observable<TimelinePosition> state{TimelinePosition{}};
};
} // namespace congregation::native
