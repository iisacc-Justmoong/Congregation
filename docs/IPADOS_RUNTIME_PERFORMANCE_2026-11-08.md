# iPadOS runtime performance and black-screen investigation

This document records observations and source analysis from 2026-11-08. It is
an investigation of the installed build, not evidence that the defects were
fixed. The device was an iPad Pro 12.9-inch (fourth generation), running
iPadOS 27.2, with the installed arm64 Release build of Congregation 1.0.2 (3).

## Device observations

- Instruments Time Profiler recorded all processes for approximately 46.2 s
  while the user drew on the canvas, changed the color picker and rapidly
  switched tools. The actions were not individually timestamped, so the trace
  does not establish a separate latency or cost for each action.
- The profiler recorded five main-thread hang intervals: 0.256 s, 0.686 s,
  1.910 s, 4.289 s and 19.101 s. The last interval continued to the end of the
  capture; its total duration and recovery time are unknown. The intervals
  occur close together, beginning approximately 20 s into the recording.
- The main thread produced 27,183 running-thread samples. In seconds 20–45 it
  produced 24,935 samples at the nominal 1 ms sampling interval, consistent
  with being CPU-busy for nearly the entire period on one core. In the first
  15 s it produced 671 samples. These are sampling estimates, not measured
  frame rates, input-to-display latency, or total process CPU utilization.
- The user subsequently observed the entire screen turn black after attempting
  to interact with the right panel. The application process remained alive.
  After a restart, the user reported that the interface initially froze and
  then turned black without an effective drag. At 16:16:28 local time the live
  console logged `iiGUI UIKit error: UIKit callback failed` followed by
  `iiGUI cause: Widget has no action`. A device screenshot showed a black
  screen and the process still appeared in the device process list.
- The user authorized discarding the test document. The app was restarted to
  restore its normal screen and was left running. The defects remain present.

The capture contains running-thread samples only. It does not measure time
spent waiting, worker queue depth, lock contention, frame presentation, GPU
utilization, memory pressure, thermal throttling, or Apple Pencil pressure.
An attempt to capture a separate brush-only profile timed out while Instruments
waited for the device, although `devicectl` still saw the USB-connected iPad.

## Findings

### 1. iOS input repeatedly materializes the full widget snapshot

In the interaction trace, at least 22,483 of the 27,183 main-thread samples
(82.7%) included `WidgetState::snapshots()` in the symbolicated call stack;
at least 21,757 (80.0%) included `accessibility_snapshots()`. The counts are
inclusive, overlap, and are lower bounds because the symbolication covered a
limited set of the most frequent instruction addresses. They must not be
added together. A color-surface input appeared in at least 17,172 samples
(63.2%); this is an inclusive stack association, not a standalone color-picker
timing. The mixed-action recording does not justify attributing all input or
brush latency to that surface.

The source explains the observed amplification:

1. `IIGUIIOSSurface::touchesMoved` forwards each UIKit coalesced touch sample
   individually. `deliver` calls `editor` before dispatch and synchronizes
   the editor afterwards.
2. `editor` requests accessibility snapshots for the widget tree and scans
   them to find the focused text editor. Accessibility change notifications
   also call `synchronizeEditor`, which can repeat this search in a guarded
   reentrant update loop.
3. `WidgetState::snapshots` calls `snapshot` for every node and recurses into
   embedded views. Each `snapshot` calls `layout_ancestry` and
   `flush_viewports`; the latter scans nodes and visits embedded views. This
   permits repeated whole-tree work, including quadratic work in a flat tree,
   even when the lookup only needs the focused editor. Snapshot text runs also
   cause font resolution work.

Relevant implementation: `SDK/iiGUI/src/Widgets/NativeWidgetsIOS.mm:39,64,82,108`,
`SDK/iiGUI/src/Widgets/WidgetState.cpp:2148,2270,2353,2459` and
`SDK/iiGUI/src/Text/FontDatabase.cpp:47,66`.

Recommended direction: provide an efficient focused-editor lookup or maintain
a correctly invalidated focused-editor cache; perform layout/viewport
synchronization once per stable revision rather than once per node; coalesce
redundant accessibility updates without removing accessibility semantics;
and avoid unnecessary font resolution. Preserve coalesced input samples and
their ordering for continuous strokes and pressure handling.

### 2. Color changes multiply synchronous UI and accessibility work

The color model publishes revisions that drive several bound widgets. The
color surfaces, alpha rail and brush tool preview can update in response to
the same model change. `WidgetState::action_options` invalidates layout and
notifies accessibility when called, and those notifications synchronously
re-enter editor synchronization. The alpha rail also builds drawing panels
per vertical row. These paths are associated with the measured color-surface
input and amplify the snapshot cost above; the recording does not isolate the
individual cost of each observer or drawing operation.

Relevant implementation: `App/Native/EditorColor.cpp:41`,
`App/Native/EditorBackend.cpp:36`, `App/Native/WorkspaceView.cpp:68,363`,
`App/Native/Application.cpp:990`, and
`SDK/iiGUI/src/Widgets/WidgetState.cpp:1663`.

Recommended direction: avoid setting identical options or model values,
batch compatible UI invalidations and accessibility notifications, and update
derived visuals at most once for the latest state per frame where semantics
permit. Measure each observer independently before further optimization.

### 3. A non-actionable hit can hide the iOS window

`WidgetState::hit_tree` can return a non-interactive widget with an input
handler. On pointer release, `WidgetState` can call `activate` on that target
without first checking that its kind is interactive. `activate` returns
`Widget has no action` for such a widget. A separate macOS headless probe using
a non-interactive input-handling container reproduced that exact error on
pointer release. This demonstrates one concrete path to the same error, but
the iPad console did not provide the actual target widget or call stack, so
the precise device trigger remains unconfirmed.

The iOS host catches a UIKit callback failure, records it and closes the host;
closing hides the `UIWindow`. This is consistent with the observed black screen
and live process. The host must distinguish a recoverable non-actionable input
from a fatal host failure; a no-op activation should not blank the application.
Fix and regression-test pointer release, input handlers, focus, cancellation,
widget lifetime and actual iOS interaction together.

Relevant implementation: `SDK/iiGUI/src/Widgets/WidgetState.cpp:1001,2978,3867`
and `SDK/iiGUI/src/Platform/HostIOS.mm:162,386`.

### 4. Core scheduling does not preempt a slow main-thread callback

The default `iiSDKCore::EventLoop` pump budget is 64 callbacks or 4 ms.
The dispatch loop checks the budget between callbacks; it cannot interrupt a
callback already executing. The iOS display callback polls the event loop on
the main thread and presents the UI, so a long synchronous input or UI update
can prevent later events and frames even with the configured pump budget.
This observation does not prove that the event-loop scheduler itself caused
the measured freeze. Its active-slot selection also scans entries; queue size,
contention and saturation require a separate workload and measurement.

Relevant implementation: `SDK/iiSDKCore/src/Runtime/Execution.h:61`,
`SDK/iiSDKCore/src/Runtime/EventLoop.cpp:316,911` and
`SDK/iiGUI/src/Platform/HostIOS.mm:275`.

In the earlier startup crash report the four iiSDKCore workers were waiting
on a condition variable. That is a snapshot of that earlier failure, not proof
of worker health during the present freeze. The current profiler capture does
not sample waiting threads and cannot establish worker starvation or health.

### 5. Other code paths to measure, not established causes

- Congregation supplies a frame loop, so the active iOS host requests
  continuous display callbacks and presents the widget tree each frame. The
  first 15 s of this trace did not show sustained main-thread CPU saturation;
  this does not establish idle GPU or energy cost.
- Nearby discovery schedules a four-second heartbeat on the event loop and
  synchronously refreshes network interfaces. A previous idle trace sampled
  this path, but did not establish its wall-clock delay or make it the cause
  of the present multi-second hang.
- Canvas painting already invalidates intersecting cached tiles, but changed
  tiles are recomposited and converted into a new RGBA image. Some selection
  and stroke-history paths copy bitmap data. iiPaintEngine can build per-pixel
  raster samples for brush dabs. These are potential brush-size, document-size
  and feature-dependent costs; the mixed trace and failed isolated recording do
  not quantify them or prove they caused the observed freeze.

Relevant implementation: `App/Native/Application.cpp:2478`,
`SDK/iiGUI/src/Platform/HostIOS.mm:275`, `App/Engine/NearbyDiscovery.h:32`,
`App/Engine/NearbyDiscovery.cpp:157,184`, `App/Native/CanvasView.cpp:36,223,414`,
`App/Native/Artwork.cpp:1653`, and
`SDK/iiPaintEngine/src/Stroke/Rasterizer.cpp:240`.

## Evidence and follow-up measurements

The local device artifacts are under
`build/ipados/performance-20261108/`: `interaction-cpu.trace`, the exported
`interaction-cpu.xml` and `interaction-hangs.xml`, plus `black-screen.png`,
`reproduced-black.png` and `restored-screen.png`. These build-directory
artifacts are local diagnostic evidence and are not a portable benchmark.

After correcting the confirmed paths, compare the same device and workload
against this capture. Separately record brush-only, color-only, tool-switch,
panel-scroll and idle workloads, along with frame and GPU timelines, memory,
thermal state, input-to-present latency, and event-loop/worker queue metrics.
Exercise VoiceOver and text focus as well as one- and two-finger gestures.
The present record is insufficient to claim that all bottlenecks are known or
that performance, black-screen recovery, or stylus pressure has been fixed.
