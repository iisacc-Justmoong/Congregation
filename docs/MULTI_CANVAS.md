# Creating documents and adding canvases

The File menu distinguishes **New document…**, which creates a separate Society
Files document using the existing new-canvas chooser, from **Add canvas…**,
which adds a finite artboard to the currently open document using that chooser.
The document panel exposes the same two actions, including on tablets. The
chooser identifies its purpose in both its heading and confirmation button; it
must not apply an addition to a different file if the active document changes
while it is open.

An additional canvas has the requested pixel dimensions, background and an
editable paint layer. The original canvas and its layers keep their positions
and pixels. The document remains finite; canvases are placed side by side with
a visible gap and the finite workspace grows only within its existing dimension
and pixel limits. Clicking a canvas with a paint tool selects an editable layer
belonging to it. Empty space between canvases does not receive brush strokes.
Existing nonnegative artboards outside the previous finite extent are included
when finding the next placement. Documents with negative-position artboards
are rejected without modification because the current viewport cannot yet
represent that workspace safely.
New paint layers inherit the active canvas; inserted fill, gradient, shape,
text and image layers retain their canvas ownership and are clipped to it.
Default image insertion uses the active canvas origin, while a drop uses the
canvas under the drop position.

The native iiSharedCanvas artboard model and `.iisc` persistence retain the
canvases and their layer ownership. Adding a canvas is a single undoable change;
undo and redo preserve layer selection, selection masks and pixels. Structural
history stores the artboard operation rather than snapshots of existing raster
assets. Rejected dimensions, unavailable history or failed durable commits leave
the original document and undo/redo stacks intact. A new document continues to
use Society's existing temporary-file and saved-file lifetime rules. Bitmap
exports flatten the whole workspace; the SDK rejects PSD export of documents
with artboards rather than silently losing their grouping.

The Figma Main page contains the LVUI File-menu example for the two actions:
nodes `470:20587` (File menu), `471:20604` (document panel) and `472:20608`
(chooser actions). Automated tests exercise creation, canvas-local painting and
tool ownership, failure rollback, undo/redo and saving and reopening without
launching the GUI.
