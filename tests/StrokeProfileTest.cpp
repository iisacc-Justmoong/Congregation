#include "../App/Native/StrokeProfile.h"

#include <iostream>
#include <stdexcept>

using namespace congregation::native;
namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
}

int main() try {
    DurationHistogram histogram;
    check(histogram.count() == 0 && histogram.percentile_upper(95) == 0, "empty histogram changed");
    histogram.add(10); histogram.add(20); histogram.add(100);
    check(histogram.count() == 3 && histogram.maximum() == 100 &&
          histogram.percentile_upper(50) >= 20 && histogram.percentile_upper(50) <= 31 &&
          histogram.percentile_upper(95) >= 100 && histogram.percentile_upper(95) <= 127,
          "histogram does not bound the measured percentiles");

    StrokeProfile disabled(false);
    disabled.begin(12, 1.0); disabled.move(4, 1.005); disabled.render(40);
    check(!disabled.active() && !disabled.finish(100, true).recorded,
          "disabled profiling admitted work");

    StrokeProfile profile(true);
    profile.begin(12, 1.0);
    profile.move(4, 1.005); profile.move(18, 1.012); profile.render(40); profile.render(75);
    auto report = profile.finish(900, true);
    check(report.recorded && report.succeeded && report.begin_us == 12 && report.finish_us == 900 &&
          report.moves == 2 && report.renders == 2 && report.move_max_us == 18 &&
          report.move_p99_upper_us >= 18 && report.render_p99_upper_us >= 75 &&
          report.render_max_us == 75 && report.max_input_gap_us >= 6999 &&
          report.max_input_gap_us <= 7001 && !profile.active(), "stroke report lost measurements");
    profile.begin(7, 2.0); profile.move(3, 1.0); profile.cancel();
    check(!profile.active() && !profile.finish(9, false).recorded,
          "cancelled profile leaked into a later stroke");
    profile.begin(2, 3.0);
    report = profile.finish(4, false);
    check(report.recorded && !report.succeeded && report.moves == 0 && report.renders == 0 &&
          report.begin_us == 2 && report.finish_us == 4, "new stroke did not reset profile state");
    profile.begin(1, 0); profile.move(1, 1000);
    check(profile.finish(1, true).max_input_gap_us == 0,
          "unknown first timestamp produced a spurious input gap");
    std::cout << "STROKE_PROFILE_OK\n";
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
