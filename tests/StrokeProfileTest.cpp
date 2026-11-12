#include "../App/Native/StrokeProfile.h"

#include <iostream>
#include <limits>
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
    DurationHistogram saturated;
    saturated.add(std::numeric_limits<std::uint64_t>::max()); saturated.add(1);
    check(saturated.total() == std::numeric_limits<std::uint64_t>::max() &&
          saturated.percentile_upper(100) == std::numeric_limits<std::uint64_t>::max(),
          "profile duration totals overflowed");

    StrokeProfile disabled(false);
    disabled.begin(12, 1.0); disabled.move(4, 1.005); disabled.render(40);
    disabled.render(StrokeRenderSample{.total_us=20, .composition_us=10, .composed_tiles=1});
    disabled.input(StrokeInputSample{.captured=true, .projection_us=5, .projected_dabs=1});
    disabled.render_ready(100); disabled.presentation_queued(110);
    check(!disabled.active() && !disabled.finish(100, true).recorded,
          "disabled profiling admitted work");

    StrokeProfile profile(true);
    profile.begin(12, 1.0);
    profile.input(StrokeInputSample{.captured=true, .projection_us=30, .tile_update_us=12,
                                    .projected_dabs=3, .projected_samples=50});
    profile.input(StrokeInputSample{.captured=false, .projection_us=200});
    profile.input(StrokeInputSample{.captured=true, .projection_us=7, .tile_update_us=3,
                                    .projected_dabs=1, .projected_samples=20});
    profile.move(4, 1.005); profile.move(18, 1.012);
    profile.render(StrokeRenderSample{.total_us=40, .composition_us=18, .conversion_us=9,
                                      .widget_publish_us=4, .composed_tiles=2, .converted_pixels=8192});
    profile.render(StrokeRenderSample{.total_us=75, .composition_us=0, .conversion_us=0,
                                      .widget_publish_us=6, .composed_tiles=0, .converted_pixels=0});
    profile.render_ready(100); profile.presentation_queued(90);
    profile.render_ready(120); profile.presentation_queued(150); profile.presentation_queued(160);
    profile.render_ready(200); profile.presentation_queued(207);
    profile.render_ready(230);
    auto report = profile.finish(900, true);
    check(report.recorded && report.succeeded && report.begin_us == 12 && report.finish_us == 900 &&
          report.moves == 2 && report.renders == 2 && report.move_max_us == 18 &&
          report.move_p99_upper_us >= 18 && report.render_p99_upper_us >= 75 &&
          report.render_max_us == 75 && report.max_input_gap_us >= 6999 &&
          report.max_input_gap_us <= 7001 && !profile.active(), "stroke report lost measurements");
    check(report.composition.count == 2 && report.composition.total_us == 18 &&
          report.composition.p50_upper_us == 0 && report.composition.p95_upper_us >= 18 &&
          report.composition.maximum_us == 18 && report.conversion.count == 2 &&
          report.conversion.total_us == 9 && report.conversion.maximum_us == 9 &&
          report.widget_publish.total_us == 10 && report.widget_publish.maximum_us == 6 &&
          report.composed_tiles == 2 && report.converted_pixels == 8192,
          "stroke report did not separate composition, conversion and widget publication");
    check(report.input_projection.count == 2 && report.input_projection.total_us == 37 &&
          report.input_projection.maximum_us == 30 && report.input_tile_update.total_us == 15 &&
          report.projected_dabs == 4 && report.projected_samples == 70,
          "stroke report lost brush projection or tile update measurements");
    check(report.presentation_queue.count == 2 && report.presentation_queue.total_us == 37 &&
          report.presentation_queue.maximum_us == 30 && report.superseded_publications == 1 &&
          report.pending_presentation,
          "stroke report matched an invalid, duplicate, or coalesced presentation");
    profile.begin(7, 2.0); profile.move(3, 1.0); profile.cancel();
    check(!profile.active() && !profile.finish(9, false).recorded,
          "cancelled profile leaked into a later stroke");
    profile.begin(2, 3.0);
    report = profile.finish(4, false);
    check(report.recorded && !report.succeeded && report.moves == 0 && report.renders == 0 &&
          report.begin_us == 2 && report.finish_us == 4 && report.composition.count == 0 &&
          report.composed_tiles == 0 && report.converted_pixels == 0 &&
          report.input_projection.count == 0 && report.projected_samples == 0 &&
          report.presentation_queue.count == 0 && report.superseded_publications == 0 &&
          !report.pending_presentation,
          "new stroke did not reset profile state");
    profile.begin(1, 0); profile.move(1, 1000);
    check(profile.finish(1, true).max_input_gap_us == 0,
          "unknown first timestamp produced a spurious input gap");
    std::cout << "STROKE_PROFILE_OK\n";
    return 0;
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
