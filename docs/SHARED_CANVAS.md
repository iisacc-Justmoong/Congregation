# iiSharedCanvas integration

Congregation's `DrawingSurfaceItem` uses `iiSharedCanvas::CanvasItem` as its native
canvas base. A document ViewModel connection creates one selected transparent
raster layer, and the existing LVRS QML painting surface binds its brush,
pressure curve, stabilizer, tool, input-state, and undo/redo properties to that
item.

The new-canvas modal also exposes an LVRS `CheckBox` for an infinite canvas.
That path creates iiSharedCanvas 1.1 `ChunkedRasterAsset` documents with a
256-pixel chunk size and the entered width/height as the small initial allocated
region. Pan and zoom camera movement maps the fixed viewport into world
coordinates and requests any newly visible region from
`ensureInfiniteCanvasRegion()`. Growth is rounded outwards to chunk boundaries;
only painted chunks store pixels.

The application discovers the installed `iiSharedCanvas` CMake package from
`$HOME/.local/SDK/iiSharedCanvas`. The package is linked alongside the current
installed iiPaintEngine; the removed legacy `CanvasAdapter` is no longer part of
the Congregation source or build contract.

The installed package version is `0.8.0` and CMake requires that exact version.
Native layers are `BitmapLayer` or `VectorLayer` alternatives; application code
reads common IDs and transforms through `iiSharedCanvas::layerProperties()`.
Keyframed test documents store their layer references in `Document::frames` and
use `KeyframedSource::frameIndices`. The existing transform and document reopen
tests verify that editing and selected raster-layer restoration keep working
with this typed layer contract.

The integration test exercises these application-level gates:

- existing Congregation brush, eraser, fill, text, shape, raster import, PNG, PSD,
  thumbnail, input-pressure, and undo/redo workflows;
- one `DrawingSurfaceItem` rendering static raster, static native vector, and a
  hold-keyframed raster layer together;
- raster text, shape, and fill operations mutating only the selected raster
  asset, including inverse mapping through its affine layer transform, without
  flattening visible vector or sibling layers into that asset;
- ordinary raster open replacing the prior mixed document even when both
  documents have the same extent;
- canonical `.iisc` save, validated decode, reopen, and raster export;
- infinite-canvas creation, signed world-origin growth, camera anchoring,
  sparse brush allocation, added-raster-layer synchronization, and native 1.1
  save/reopen;
- a fresh CMake configure selecting the installed iiPaintEngine and
  iiSharedCanvas packages rather than the former build-local legacy prefix.

`DrawingSurfaceItem::saveToFile()` and `openRaster()` accept `.iisc` for the
native document owned by the item. Writes use `QSaveFile`; reads pass through
the iiSharedCanvas checksum, allocation-limit, canonical-form, and document
validation gates before replacing the current document.

The rendered mixed frame is the display and flat-export boundary only. Raster
authoring starts from `CanvasItem::selectedRasterPixels()`, maps document-space
coordinates through the selected layer's inverse affine transform, and commits
with `replaceSelectedPixels()`. This keeps native vector, sibling raster, and
other-frame content separate while editing. Opening an ordinary bitmap is an
explicit new-document operation and therefore replaces, rather than partially
mutates, any previously opened mixed document.

iiSharedCanvas display tiles now render asynchronously. Congregation's flat-file
export and embedded raster-layer snapshot paths therefore render the current
authoritative document synchronously with `renderFrameRegion()` instead of
assuming `CanvasItem::framePixels()` is already populated. Display scheduling
cannot make a just-saved file empty or stale.

When an infinite region grows left or above its prior origin, QML shifts
session objects by the reported margin and expands every added raster-layer
item to the same origin and extent. It adjusts the center-origin pan offset by
the asymmetric growth, so a world point keeps its screen position during the
structural resize. Existing finite-canvas behavior remains unchanged.
Full-canvas raster-layer delegates bind their visual and pointer geometry
directly to the base canvas surface rather than cached session dimensions, so
every visibly allocated pixel remains drawable during asynchronous model
updates and recent-session restoration.

The application open dialog includes `.iisc`, so a mixed raster/vector/timeline
document can be selected and displayed by the Congregation canvas. The Save As
dialog deliberately does not advertise `.iisc` yet: Congregation's existing QML
session-layer stack must first be mapped into native assets so a normal layered
editing session cannot appear to save while losing content.

The existing `DrawingSurface.qml` still owns its image, text, shape, and extra
raster-layer delegates as product-session objects. They continue to work and
remain covered by the existing workflow tests, but they are not silently
omitted or flattened into `.iisc`: the composite save overload fails closed
when such objects are present. Mapping those session objects to native
iiSharedCanvas assets is a separate product migration, not a prerequisite for
using the mixed canvas base.

Recent canvas persistence is deliberately a separate application-session
boundary rather than pretending those QML objects are already native `.iisc`
assets. `recent-canvas.vrc` wraps the validated iiSharedCanvas document plus
PNG bytes for inserted images and additional raster layers and JSON-safe
text/shape/object metadata. It is limited to one owner-only file below
`QStandardPaths::AppLocalDataLocation/canvas`, uses a versioned header and
SHA-256 payload check, and replaces the previous snapshot through `QSaveFile`.
The native snapshot extent is normalized to the live visual canvas size before
encoding. The page schedules it 1.2 seconds after the latest session mutation
and flushes a pending edit during normal window close. Decode and every embedded
image are validated before the current native document is replaced. Valid PNGs
are extracted only to one owner-only temporary directory held by the restored
surface and are removed with that surface, so the `.vrc` remains the sole
persistent recent-session artifact.

## Local-network canvas sessions

Preferences → Members exposes explicit **Share canvas**, **Join nearby…**, and
**Stop sharing/Leave canvas** actions. Its `+` menu can also invite a specifically
selected nearby Congregation user who enabled **Allow inviting other users**. The
anonymous heartbeat carries no profile or canvas bytes; it adds only an
invitation-capability Boolean and the host's temporary TCP port while sharing is
active. Selecting an invitee sends a bounded, target-session-addressed one-hop
datagram with an invitation UUID, temporary endpoint, and normalized inviter
profile name. It contains no profile image, account, device name, or document
data. Duplicate invitation IDs are suppressed across network interfaces.

The recipient queues at most 16 invitations and shows the first through the
toolbar Profile button's notification badge and LVRS context menu. **Accept**
passes `true`, removes the invitation, and joins the advertised endpoint;
**Decline** passes `false` and removes it without connecting. Disabling
invitations clears the queue. Canvas profile lists and document data otherwise
cross the network only after a client joins. This is direct, unencrypted LAN TCP
with no Internet relay or shared secret, so the feature is intended for a
trusted local network.

Only the host transfers the same complete, SHA-256-checked `.vrc` bytes used by
recent-session persistence. This includes the canonical iiSharedCanvas
document, additional raster-layer and inserted-image PNGs, editable text/shape
metadata, object ordering, and background presence. QML exports it in memory;
no temporary session file is needed for transport.

The joined surface is an input client for the host canvas, not a second document
owner. It remains command-blocked until the first authoritative host state has
finished its queued QML restore. Pointer/tablet strokes are then captured with
their pressure samples and current brush style while iiSharedCanvas renders the
same active stroke as a local-only live preview. Releasing the pointer cancels
that preview back to the exact pre-stroke pixels and history before the bounded
stroke command is sent to the host, so it never becomes a client-owned edit.
Fill, text, shape, transform, canvas, layer, undo/redo, and validated
raster/image actions follow the same semantic-command path without speculative
document mutation. The client cannot publish a `.vrc` snapshot and does not
store a remote session as its Recent canvas.

The host validates each command, applies it to the actual host
`DrawingSurface`, and only then exports and broadcasts an authoritative `.vrc`
state with the next monotonically increasing revision. A zero-interval queued
publication lets newly created QML raster-layer surfaces exist before export
and coalesces commands processed in the same event cycle. The host's own local
edits retain the 350 ms publication debounce. Other devices therefore receive
a participant stroke only after release, while its originating device sees the
non-committing preview during input. Clients always restore host states,
including the state produced from their own input.

This is not an object-level CRDT. Commands are serialized in arrival order by
the host canvas; overlapping absolute transforms therefore resolve to the last
host-applied command. Host snapshots remain the sole canonical state and repair
any stale participant view.

Protocol frames are versioned and size bounded, non-LAN IPv4 addresses are
rejected, and connection or incomplete-handshake attempts time out after eight
seconds. Peer/session UUIDs and profile names are validated, duplicate peers
are rejected, and hosts accept at most 16 remote participants. The host
can remove a participant, and stopping sharing disconnects all clients while
withdrawing the discovery port.

Large-document memory budgets, partial host-state deltas, encrypted/authenticated
LAN sessions, object-level concurrent merge, and physical multi-device package
validation remain product-hardening gates.

Session export normalizes the extent of a document copy to the visible canvas
size. It preserves the source authorship ledger after that normalization, since
exporting or transmitting an unchanged document is not an edit. Repeated
snapshots therefore retain identical bytes, revisions, and timestamps, including
a late view resize. The resize/session regression checks both repeated export
and the original document ledger.
