#include <Canvas/Artwork.h>
#include <Collaboration/CanvasCollaboration.h>
#include <iiFileProvider>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace congregation;
using namespace congregation::native;
using namespace iiSharedCanvas;
using namespace std::chrono_literals;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> bool wait(ii::runtime::EventLoop& loop, F predicate) {
    const auto end=ii::runtime::Clock::now()+60s;
    while (!predicate() && ii::runtime::Clock::now()<end) { loop.poll(); std::this_thread::sleep_for(1ms); }
    return predicate();
}
CanvasCollaboration::DocumentPort port(Artwork& artwork, int& checkpoints) {
    return {[&artwork, &checkpoints] { ++checkpoints; return artwork.snapshot(); },
            [&](std::string_view bytes) { return artwork.receive(bytes); },
            [&](std::string_view bytes) { return artwork.receive_edit(bytes); }};
}
void stroke(Artwork& artwork, double x, double y) {
    BitmapBrush brush; brush.size=6; brush.argb=0xff4433bb;
    if (!artwork.begin({x,y},1,brush) || !artwork.end({x+8,y+4},1))
        throw std::runtime_error("Stroke failed: " + artwork.error());
}
void run() {
    iiFileProvider::native::TemporaryDirectory directory(std::filesystem::temp_directory_path());
    check(directory.isValid(), "test directory unavailable");
    ii::runtime::EventLoop loop;
    Artwork host_doc(directory.path()), guest_doc(directory.path());
    if (!host_doc.create(256,256)) throw std::runtime_error("Host setup failed: " + host_doc.error());
    if (!guest_doc.create(16,16)) throw std::runtime_error("Guest setup failed: " + guest_doc.error());
    CanvasSession host(loop,uuid()), guest(loop,uuid());
    int host_checkpoints = 0, guest_checkpoints = 0;
    CanvasCollaboration host_sync(host,port(host_doc,host_checkpoints)), guest_sync(guest,port(guest_doc,guest_checkpoints));
    host_doc.document_changed=[&](const DocumentChange& c) { host_sync.local_change(c); };
    guest_doc.document_changed=[&](const DocumentChange& c) { guest_sync.local_change(c); };
    host.changed=[&](auto n) { host_sync.session_changed(n); };
    guest.changed=[&](auto n) { guest_sync.session_changed(n); };
    check(host.host("host") && host.initialize(host_doc.snapshot()), "host setup failed");
    check(guest.join(host.session,"127.0.0.1",host.port(),"guest"), "guest join failed");
    check(wait(loop,[&] { return guest.revision==1 && guest_doc.snapshot()==host_doc.snapshot(); }),
          "initial document was not synchronized");
    check(guest_doc.set_selection(SelectionGeometry::Rectangle,{0,0},{24,24}), "guest selection failed");
    const auto selection=guest_doc.selection_mask_handle();
    stroke(host_doc,40,40);
    check(wait(loop,[&] { return guest.revision==2 && guest_doc.snapshot()==host_doc.snapshot(); }),
          "host stroke was not replicated");
    check(guest_doc.selection_mask_handle()==selection, "remote stroke changed guest selection");
    check(guest_doc.clear_selection(), "guest selection clear failed");
    stroke(guest_doc,80,80); stroke(guest_doc,120,120);
    check(wait(loop,[&] { return guest.revision==4 && host.revision==4 && guest_doc.snapshot()==host_doc.snapshot(); }),
          "successive optimistic guest strokes were lost or reordered");
    check(host_doc.undo(), "host undo failed");
    check(wait(loop,[&] { return guest.revision==5 && guest_doc.snapshot()==host_doc.snapshot(); }),
          "host undo did not replicate as a pixel change");
    const auto host_checkpoint_count = host_checkpoints, guest_checkpoint_count = guest_checkpoints;
    const auto layer = host_doc.selected();
    check(host_doc.rename(layer, "Shared layer"), "host layer rename failed");
    check(wait(loop,[&] { return guest.revision==6 && guest_doc.snapshot()==host_doc.snapshot(); }),
          "layer rename was not replicated");
    check(guest_doc.rename(layer, "Guest layer"), "guest layer rename failed");
    check(wait(loop,[&] { return host.revision==7 && guest.revision==7 && guest_doc.snapshot()==host_doc.snapshot(); }),
          "guest layer rename was not replicated");
    check(host_doc.undo(), "host layer rename undo failed");
    check(wait(loop,[&] { return guest.revision==8 && guest_doc.snapshot()==host_doc.snapshot(); }),
          "layer rename undo was not replicated");
    check(host_checkpoints == host_checkpoint_count && guest_checkpoints == guest_checkpoint_count,
          "a layer property edit serialized a full checkpoint");
    // Timing and frame-owned assets also travel as ordered edits.
    check(host_doc.timing(24, 2), "host timeline change failed");
    check(wait(loop,[&] { return guest.revision==9 && guest_doc.snapshot()==host_doc.snapshot(); }),
          "timeline event was not replicated");
    stroke(host_doc, 160, 160);
    check(wait(loop,[&] { return guest.revision==10 && guest_doc.snapshot()==host_doc.snapshot(); }),
          "frame-copy event was not replicated");
    check(host_checkpoints == host_checkpoint_count && guest_checkpoints == guest_checkpoint_count,
          "timing or frame-owned asset edits serialized a full checkpoint");
    guest.set_profile("renamed guest");
    check(wait(loop,[&] {
        return std::ranges::any_of(host.profiles, [](const auto& p) {
            const auto object = p.template get<Object>();
            return object && text(field(object->get(), "profileName")) == "renamed guest";
        });
    }), "profile event was not replicated");
    check(host_checkpoints == host_checkpoint_count && guest.revision == 10,
          "a profile event initiated a new document checkpoint");
    auto next = guest.revision;
    const auto advances = [&](const char* reason) {
        ++next;
        check(wait(loop, [&] { return guest.revision == next && host.revision == next; }), reason);
        check(guest_doc.snapshot() == host_doc.snapshot(), "runtime event diverged");
    };
    check(host_doc.add_layer("Inserted"), "layer insertion failed");
    advances("layer insertion was not replicated");
    const auto inserted = host_doc.selected();
    check(host_doc.move(inserted, layer) && host.revision == next, "unchanged layer order emitted an edit");
    check(host_doc.move(layer, inserted), "layer ordering change failed");
    advances("layer move was not replicated");
    check(guest_doc.select(inserted) && guest_doc.remove_layer(), "guest layer deletion failed");
    advances("layer deletion was not replicated");
    check(guest_doc.undo(), "structural undo failed");
    advances("layer deletion undo was not replicated");
    check(host_doc.reframe_canvas({{8,8},{224,224}}), "crop failed");
    advances("crop was not replicated");
    check(host_checkpoints == host_checkpoint_count && guest_checkpoints == guest_checkpoint_count,
          "a structural edit restarted document synchronization");

    CanvasSession late(loop, uuid());
    Artwork late_doc(directory.path());
    check(late_doc.create(32,32), "late peer setup failed");
    int late_checkpoints = 0;
    CanvasCollaboration late_sync(late, port(late_doc, late_checkpoints));
    int initialized = 0;
    late.snapshot_received = [&](auto, auto, auto) { ++initialized; };
    const auto durable = host_doc.snapshot();
    BitmapBrush pending; pending.size=6; pending.argb=0xff71aa22;
    check(host_doc.begin({16,16},1,pending) && host_doc.extend({24,24},.8), "pending host stroke failed");
    check(host_doc.snapshot() == durable, "a checkpoint included uncommitted stroke pixels");
    check(late.join(host.session,"127.0.0.1",host.port(),"late"), "late peer join failed");
    check(wait(loop, [&] { return late.revision == next && initialized == 1; }), "late peer did not initialize");
    check(late_doc.snapshot() == host_doc.snapshot() && host_checkpoints == host_checkpoint_count+1 &&
          guest.revision == next, "late initialization disturbed an existing participant");
    host_doc.cancel();
    check(host_doc.redo() == false, "a new crop should clear the redo stack");
    check(host_doc.undo(), "crop undo failed");
    advances("crop undo did not replicate");
    check(wait(loop, [&] { return late.revision == next; }) && initialized == 1 &&
          late_doc.snapshot() == host_doc.snapshot(), "late peer did not transition to event-only operation");
    late.stop();
    guest.stop(); host.stop(); loop.poll();
}
}
int main() {
    try { run(); std::cout << "COLLABORATIVE_SESSION_INTEGRATION_OK\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
