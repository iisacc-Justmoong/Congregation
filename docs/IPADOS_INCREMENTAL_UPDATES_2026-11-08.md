# iPadOS incremental update follow-up

This record follows the earlier iPadOS runtime investigation on 2026-11-08.
It separates implemented update paths and automated checks from observations
on the installed device; it is not a painting-latency benchmark.

## Update paths

iiGUI follows the focused editor path for iOS text-input synchronization.
Full accessibility enumeration remains synchronous, but shares viewport and
layout synchronization within bounded groups. Unchanged action options do not
invalidate the view, custom widget drawings are retained per widget, and an
explicit repaint invalidates the requested subtree. A non-actionable input
target no longer invokes a nonexistent action on release.

On Metal, opaque frames without nonlocal effects retain the previous scene
texture and recompose the changed, disjoint regions, with a limit of 16 region
passes. Resize, background changes, scene sources, shadows and backdrops
conservatively use full composition. The final drawable copy still covers the
frame, and other renderers do not inherit Metal's partial composition. The
widget frame builder still traverses and assembles built-in drawing commands;
the retained widget drawing applies to custom providers. These boundaries do
not establish fully incremental behavior for every part of the application.

The LVUI color integration caches a color surface by the values its raster
actually depends on. It keeps current selection and marker state while reusing
unchanged pixels; the value-returning API still copies the RGBA buffer.

## Verification

- The final iiGUI focused update, damage, subtree and Metal checks passed 9/9.
  The Metal check compared partial frames with fresh full compositions at the
  pixel level, including two separated changed areas. The LVUI color tests
  passed 3/3. A wider earlier iiGUI selection passed 89/90; the remaining
  `iiGUI.self_widgets_compound` check reported a segmented-style contract
  mismatch in the existing working tree.
- The iPad SDK and Congregation arm64 Release were rebuilt, and the signed
  1.0.2 (3) app was installed and launched on the connected fourth-generation
  12.9-inch iPad Pro. The process remained present, and screenshots at launch
  and after the recorded interval showed the same non-black application view.
  In a later user exercise the canvas acquired visible brush marks, and the
  selected tool, color and controls changed while the same process remained
  alive. The visible changes appeared after the recorded interval; the timing
  of any interactions without visible changes is not established.
- A 46.3-second Time Profiler recording contained no potential-hang rows.
  User interaction during the capture was not confirmed and the two
  screenshots were byte-identical. The recording therefore does not validate
  brush or panel interaction and is not comparable to the earlier mixed-input
  trace. It does not prove that latency or the reported black screen is fixed.
  A second recording attempted after the later user exercise timed out while
  Instruments waited for the iPad to boot; CoreDevice still listed the app
  process. No interaction profile was obtained from that attempt.

The local artifacts are in `build/ipados/performance-20261108/`, including
`incremental-interaction.trace`, its exported tables, and
`after-incremental.png`, `after-interaction.png` and `after-user-reply.png`.
A further same-workload
device recording is required to compare input latency, hangs, GPU work and
panel scrolling against the earlier trace.
