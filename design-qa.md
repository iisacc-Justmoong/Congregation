# Design QA: 2px white color-picker border

## Comparison target

- Source visual truth: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-1c734e3e-84ed-42cc-8366-176a144f21f2.png`, refined by the user's explicit requirement that the current-color circle have a 2px white border so black remains visible.
- Component: the current-color `LV.IconButton` at the far right of `CanvasToolBar.qml`.
- Required states:
  - Black current color, picker closed, to verify minimum-contrast visibility.
  - Magenta current color, picker closed, to compare against the supplied visual.
  - Black current color, picker open, to verify the primary interaction.

## Evidence and normalization

- Source visual:
  - Path: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-1c734e3e-84ed-42cc-8366-176a144f21f2.png`
  - Pixel size: 57 x 83 at 1x.
- Pre-fix black implementation:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-borderless-full.png`
  - Logical viewport: 1280 x 853.
  - Pixel size: 2560 x 1706 at 2x Retina.
- Post-fix black implementation:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-black-full.png`
  - Logical viewport: 1280 x 853.
  - Pixel size: 2560 x 1706 at 2x Retina.
- Post-fix magenta implementation:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-magenta-full.png`
  - Logical viewport: 1280 x 853.
  - Pixel size: 2560 x 1706 at 2x Retina.
- Picker-open implementation:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-black-picker-open.png`
- Density-normalized focused captures:
  - Black before: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-borderless-black-normalized.png`
  - Black after: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-black-normalized.png`
  - Magenta after: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-color-picker-white-border-magenta-normalized.png`
  - Each focused capture was downsampled from the same 2x Retina crop to 57 x 83 at 1x.
- Combined comparison inputs:
  - Black visibility, left before and right after: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/color-picker-black-before-vs-white-border-after.png`
  - Supplied source, left, and magenta implementation, right: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/color-picker-source-vs-white-border-implementation.png`
  - Both inputs place equal 57 x 83 crops together before 4x inspection scaling.

## Findings

- No actionable P0, P1, or P2 finding remains.
- The black current-color fill is now visibly bounded by the requested white ring against the dark toolbar.
- The control keeps one color circle and one visibility border. The previous outer button frame and decorative ring do not return.
- The white border is specified as `LV.Theme.scaleMetric(2)` and resolves to exactly 2 logical pixels in the verified desktop profile.

## Required fidelity surfaces

- Fonts and typography: passed; this component has no text content, and adjacent toolbar typography is unchanged.
- Spacing and layout rhythm: passed; the stock 22 x 22 LVRS button frame and toolbar alignment are unchanged, while the border is drawn inward inside the stock icon slot.
- Colors and visual tokens: passed; fill remains bound to `toolbar.currentColor`, and the visibility border is opaque `#ffffff` as requested. Black and magenta states were both observed.
- Image quality and asset fidelity: passed; the dynamic color indicator remains an antialiased QML control surface at native Retina density, with no raster scaling artifact or replacement icon asset.
- Copy and content: passed; the `Brush color` accessible name and tooltip are unchanged.

## Interaction and accessibility

- Activating the exposed `Brush color` AX button opens the HSL triangle picker.
- HSL selection updates the toolbar indicator immediately while preserving the 2px white border.
- Escape closes the picker, and the stock LVRS `Borderless` hover, press, focus, and hit-target behavior remains available.

## Comparison history

1. The earlier magenta-only pass removed every outline and passed its focused comparison.
2. The newly required black-state comparison exposed a P1 affordance issue: the borderless black circle nearly disappeared into the dark toolbar.
3. The current-color circle received an inward 2px opaque white border without restoring the removed outer frame or decorative ring.
4. The post-fix black comparison shows a clearly visible circle, and the post-fix magenta/source comparison confirms the simplified single-ring structure. No P0/P1/P2 issue remains.

## Implementation checklist

- [x] Preserve stock `LV.IconButton` geometry and `Borderless` tone.
- [x] Preserve `toolbar.currentColor` fill.
- [x] Add a 2px `#ffffff` border to the color circle only.
- [x] Keep the outer frame and decorative ring removed.
- [x] Preserve tooltip, accessibility name, and HSL picker interaction.
- [x] Verify black and non-black states in the running 1280 x 853 app.

## Toolbar horizontal padding follow-up

- The full-width toolbar background and bottom separator remain flush with the 1280-pixel window.
- The toolbar content layout uses `LV.Theme.gap16` for both `anchors.leftMargin` and `anchors.rightMargin`; no stock LVRS button dimension is overridden.
- In the rebuilt running app, the window frame was `100,80,1280,853`, the 22 x 22 `New canvas` button frame began at `116,115`, and the 22 x 22 `Brush color` button frame began at `1342,115`. The resulting left and right edge gaps are both exactly 16 logical pixels.
- Closed-state edge evidence: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-toolbar-horizontal-padding-16-edges.png`.
- Open-state interaction evidence: `/Users/ymy/.codex/visualizations/2026/08/17/01a00d9d-b955-71b3-b20c-e14c2686105e/congregation-toolbar-horizontal-padding-16-color-picker-open-crop.png`; the picker remains visible and aligned within the window after the right inset.

final result: passed

# Design QA: Profile avatar icon centering

## Comparison target

- Source visual truth: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-0aecfb71-e282-4600-90d9-54c04cbda3fa.png`.
- Component: the empty-profile `LV.IconButton` in `PreferencesWindow.qml`.
- Required state: dark Preferences window with the profile image button hovered so its 64 x 64 circular frame is visible.

## Evidence and normalization

- Source visual:
  - Pixel size: 86 x 107 at 1x.
  - The user icon center was x=51 within the supplied crop, while the circular frame center was x=59.
- Post-fix full view:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-alignment-after-hover.png`.
  - Logical viewport and pixel size: 480 x 360 at 1x.
- Post-fix focused view:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-alignment-after-hover-focused.png`.
  - Pixel size: 86 x 107 at 1x; no density normalization was required.
- Combined source-left / implementation-right comparison:
  - Path: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-alignment-source-vs-after.png`.
  - 4x nearest-neighbor inspection: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-alignment-source-vs-after-4x.png`.

## Findings

- The pre-fix implementation had a P2 horizontal-alignment drift: the stock user icon was 8 logical pixels left of the circular button frame center.
- Symmetric 10-pixel content padding now centers the 44 x 44 icon within the 64 x 64 frame.
- The post-fix icon center and circular frame center both resolve to x=59 in the focused comparison. No actionable P0, P1, or P2 finding remains.

## Required fidelity surfaces

- Fonts and typography: passed; profile labels and text-field typography are unchanged.
- Spacing and layout rhythm: passed; the 64 x 64 profile frame and surrounding Preferences layout are unchanged, while only the icon's content inset changed.
- Colors and visual tokens: passed; existing LVRS theme colors, hover treatment, and borderless button tone are unchanged.
- Image quality and asset fidelity: passed; the stock LVRS `user.svg` remains in use at 44 x 44 with no raster replacement or scaling artifact.
- Copy and content: passed; visible section copy is unchanged, while the profile button's accessible name now reflects its options-menu role.

## Interaction and accessibility

- Activating the exposed `Profile image options` AX button opens the LVRS profile-image context menu.
- `Select profile image` opens the native `Choose profile image` file dialog on the next event-loop turn; Escape closes either surface and returns focus to Preferences.
- The profile button remains borderless outside the stock LVRS hover/focus treatment.

## Comparison history

1. Source inspection established an 8-pixel left shift between the user icon and circular frame centers.
2. The retained LVRS default 2-pixel horizontal padding left a 60-pixel content area and placed the 44-pixel icon from that area's leading edge.
3. The implementation now derives a 10-pixel symmetric inset from `(avatarSize - iconSize) / 2`, keeping the sizes and stock asset unchanged.
4. The rebuilt and hovered control has coincident icon and frame centers, and its file-dialog interaction remains intact.

final result: passed

# Design QA: Native-resolution circular profile crop

## Comparison target

- Component: the selected-image state of the borderless profile `LV.IconButton` in `PreferencesWindow.qml`.
- Input: `/Volumes/Storage/Workspace/Product/Congregation/docs/marketing/congregation-windows-editor.png`.
- Required state: the selected landscape image must be centered, cropped to a 1:1 square, masked to a circle, and displayed without changing the existing 64-DIP button geometry.

## Evidence and pixel contract

- Input size: 1403 x 911 pixels.
- Processed image evidence: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-center-crop-output-911.png`.
- Processed size: 911 x 911 pixels, matching the input's shorter side with no additional resize.
- Processed format: lossless PNG with alpha; transparent corners and the antialiased circular edge are present.
- Running Preferences evidence: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-center-crop-live.jpeg`.

## Findings

- The previous generic LVRS icon path used `Image.PreserveAspectFit`, which reduced the entire photograph into the icon box and did not crop it.
- `ProfileImageProcessor` now applies orientation metadata, selects the largest centered source square, draws it into an equal-sized output without scaling, and applies an antialiased circular alpha clip.
- Landscape and portrait regression fixtures prove that the output side equals the source's shorter side and that output center pixels map to the corresponding source center pixels.
- No actionable P0, P1, or P2 finding remains.

## Interaction and accessibility

- Activating `Profile image options`, then `Select profile image`, opens the native image chooser.
- Choosing the 1403 x 911 image returns to Preferences and immediately replaces the stock user icon with the circular preview.
- The button's borderless tone, 64-DIP frame, and centered 44-DIP content area remain unchanged; its accessible name now describes the intermediate options menu.
- A failed subsequent image decode retains the previous valid crop.

final result: passed

# Design QA: Profile image context menu

## Required flow

- Pressing the profile image must open a stock LVRS context menu rather than the native file chooser directly.
- The menu must always show `Select profile image` and `Delete profile image`.
- Select must open the native chooser only after the context menu closes.
- Delete must clear the preview and remove its temporary circular PNG; it remains visible but disabled when no image exists.

## Evidence

- Registered-image menu: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-image-context-menu-final.jpeg`.
- Deleted-image state: `/Users/ymy/.codex/visualizations/2026/08/19/01a01793-fcb7-7a52-85d2-f1acf4e7648c/profile-image-after-delete.jpeg`.
- The final running accessibility tree exposed `Profile image options`, `Select profile image`, and `Delete profile image` by name; Delete was disabled in the empty-image state and enabled after selection.

## Findings

- The direct `onClicked: profileImageDialog.open()` path has been removed.
- `ContextMenu.openFor()` anchors the popup to the existing borderless profile button without adding another frame.
- A stock `LV.MenuItem` delegate preserves LVRS styling while binding each visible label to its accessibility name.
- The Select path reached `Choose profile image`; the Delete path restored the centered stock user icon and left no `Congregation-profile-image-*.png` temporary file.
- No actionable P0, P1, or P2 finding remains.

final result: passed

# Design QA: Presentation laser single-dot and smooth-trajectory correction

## Evidence and normalization

- Source visual truth: `/var/folders/j3/5qyn3r610nsfxzwk8_q8w9gm0000gn/T/codex-clipboard-fcb57daa-3e04-4f94-b5f2-cb22826fba5b.png`, together with the explicit requirement that the cursor position own exactly one dot and the trajectory own none.
- Source dimensions: 1588 × 1026 px.
- Pre-fix deterministic reproduction: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/busy-path-before-fix.png`, 1366 × 768 px.
- Pre-smoothing deterministic reproduction with the corrected point ownership but `lineTo` trajectory: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/busy-path-before-smoothing.png`, 1366 × 768 px.
- Post-fix active-pointer implementation: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/busy-path-after-fix-active.png`, 1366 × 768 px.
- Post-fix released-pointer implementation: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/busy-path-after-fix-released.png`, 1366 × 768 px.
- Compiled `build/Congregation.app` released-pointer evidence: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/compiled-app-released-no-dots.jpg`, 1366 × 768 px.
- Full-view three-state comparison: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/laser-pointer-single-dot-comparison.png`, 4098 × 768 px.
- Controlled polyline/active-curve/released-curve comparison: `/Users/ymy/.codex/visualizations/2026/08/24/01a03156-7a59-7700-8094-3c88247fe505/laser-pointer-smooth-curve-comparison.png`, 4098 × 768 px.
- Viewport and density: the implementation component was rendered at 1366 × 768 logical and physical pixels with the Qt offscreen software backend. The 1588 × 1026 source was proportionally scaled and center-cropped to 1366 × 768 before comparison.
- Compared state: several recent, crossing laser gestures in presentation mode; the middle panel retains the active cursor and the right panel represents the exact same trail immediately after release.
- Focused evidence: the active-pointer implementation is large enough to inspect every join, cap, intersection, and the sole cursor endpoint without a separate crop.

## Findings

- [Resolved P1] The prior implementation used round joins in 24 opacity passes. Curved or rapidly changing paths therefore reconstructed a circular core and glow at many sampled vertices even though explicit `arc()` calls had been removed.
- [Resolved P1] Removing the sample dots exposed a separate `lineTo` polyline defect: sparse event samples appeared as joined straight sections and could not express the continuously curved mouse trajectory.
- Every contiguous run now uses Catmull–Rom-derived cubic Bézier segments whose endpoints are the sampled mouse coordinates. The controlled comparison shows the same input coordinates changing from angular joins to continuous curves.
- The trail retains butt caps and bevel joins. No circular cap, join, arc, fill, or repeated point item remains in the trajectory.
- `activeLaserPoint` is instantiated once and positioned only from `currentPointX`/`currentPointY`. The active capture contains exactly one composite laser dot at that cursor coordinate.
- The released capture contains zero dots. An exact duplicate release coordinate is rejected before it can create a zero-length terminal segment.
- The rebuilt native app was relaunched, entered Presentation mode, and received four independent full-canvas drags. Its released-state capture retained only fading strokes with no red endpoint or sample dots.
- Typography and copy: not applicable to this pointer-only overlay; no text surface changed.
- Spacing and layout rhythm: the full-canvas overlay geometry and 6 px core/16 px glow widths are unchanged.
- Colors and tokens: the existing `#ff2b2b` laser color and opacity envelope are unchanged.
- Image quality: antialiased core and glow strokes remain sharp at 1×; intersections may naturally accumulate stroke color, but no intersection is represented by a point primitive.

## Comparison history

1. The earlier QA pass used a straight drag, which concealed the round-join defect and was incorrectly marked passed.
2. The current user-provided busy-path source exposed the remaining P1 repeated-dot mismatch.
3. The deterministic pre-fix reproduction confirmed circular join geometry at path reversals.
4. Round joins were replaced with bevel joins, the flat cap was retained, and duplicate zero-length release samples were rejected.
5. The post-fix active and released captures show exactly one cursor-owned dot and then zero dots, with no trajectory-owned dots; trajectory fidelity was still unresolved at this stage.
6. A further report identified the remaining angular polyline effect, so the point-corrected render was retained as a controlled pre-smoothing baseline.
7. The same input samples were rerendered through cubic Bézier interpolation. Bends now follow a continuous curve while the active point remains attached to the unmodified current cursor coordinate.
8. The released curve capture retains zero dots. No actionable P0/P1/P2 finding remains.

final result: passed
