#include <Canvas/Artwork.h>
#include <iiFileProvider>
#include <iostream>
#include <stdexcept>

using namespace congregation::native;
using namespace iiSharedCanvas;

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
Document fixture() {
    Document d;
    d.extent = {256, 256};
    d.assets.emplace_back(RasterAsset{"paint", makeRasterLayer(256, 256, 0)});
    d.assets.emplace_back(RasterAsset{"other", makeRasterLayer(256, 256, 0xffaabbcc)});
    d.layers.emplace_back(StaticBitmapLayer{{"paint-layer", "Paint"}, StaticSource{"paint"}});
    d.layers.emplace_back(StaticBitmapLayer{{"other-layer", "Other"}, StaticSource{"other"}});
    return d;
}
void run() {
    iiFileProvider::native::TemporaryDirectory temp(std::filesystem::temp_directory_path());
    check(temp.isValid(), "temporary fixture unavailable");
    {
        Artwork receiver(temp.path());
        auto initial = fixture(), checkpoint = initial;
        findRasterAsset(checkpoint, "paint")->pixels.pixels[17] = 0xffaacc22;
        if (!receiver.replace(initial)) throw std::runtime_error("Checkpoint fixture failed: " + receiver.error());
        const auto before = receiver.snapshot();
        const auto encoded = encodeIisc(checkpoint);
        check(encoded.ok(), "checkpoint encoding failed");
        const std::string bytes(reinterpret_cast<const char*>(encoded.bytes.data()), encoded.bytes.size());
        check(receiver.receive(bytes) && receiver.snapshot() == bytes,
              "checkpoint did not preserve exact authorship");
        check(!receiver.can_undo() && before != bytes,
              "initialization inserted the previous document into runtime history");
    }
    Artwork source(temp.path()), receiver(temp.path());
    check(source.replace(fixture()) && receiver.replace(fixture()) && source.select("paint-layer"),
          "document fixture failed");
    const auto receiver_selected = receiver.selected();
    const auto before = receiver.snapshot();
    check(receiver.set_selection(SelectionGeometry::Rectangle, {0, 0}, {20, 20}), "selection setup failed");
    const auto selection = receiver.selection_mask_handle();
    DocumentChange change;
    source.document_changed = [&](const auto& value) { change = value; };
    BitmapBrush brush; brush.size = 8; brush.argb = 0xff273bd1U;
    if (!source.begin({32, 40}, 1, brush) || !source.extend({48, 48}, .7) || !source.end({55, 52}, .5))
        throw std::runtime_error("Source stroke failed: " + source.error());
    const auto wire = [&] {
        check(change.edit && !change.edit->empty(), "the operation did not publish an event");
        return std::string(change.edit->begin(), change.edit->end());
    };
    const auto event = wire();
    check(change.impact && change.impact->pixels.size() == 1 &&
          change.impact->pixels.front().asset == "paint" && change.impact->layers.empty(),
          "stroke did not identify the changed asset tiles");
    check(event.size() < 256 * 256 * 4, "pixel change sent the entire canvas");
    const auto revision = receiver.revision();
    const auto* untouched = findRasterAsset(receiver.document(), "other")->pixels.pixels.data();
    check(!receiver.receive_edit(std::string_view(event).substr(0, event.size()-1)) &&
          receiver.snapshot() == before && receiver.revision() == revision,
          "truncated edit changed the working document");
    check(receiver.receive_edit(event), "receiver rejected a valid patch");
    check(receiver.revision() == revision + 1 && receiver.snapshot() == source.snapshot() &&
          findRasterAsset(receiver.document(), "other")->pixels.pixels.data() == untouched,
          "receiver did not retain unchanged raster storage");
    check(receiver.selected() == receiver_selected && receiver.selection_mask_handle() == selection,
          "remote edit changed local selection");
    const auto applied = receiver.snapshot();
    check(receiver.undo() && receiver.snapshot() == before && receiver.redo() && receiver.snapshot() == applied,
          "remote edit did not preserve undo/redo");
    check(source.undo(), "source undo failed");
    const auto undo = wire();
    namespace sql = iiFileProvider::native::sql;
    auto database = sql::Database::open(receiver.recovery());
    check(bool(database), "cannot open receiver working file");
    auto lock = sql::Transaction::begin(*database, true);
    check(bool(lock), "cannot lock receiver working file");
    const auto locked = receiver.snapshot(); const auto locked_revision = receiver.revision();
    check(!receiver.receive_edit(undo) && receiver.snapshot() == locked &&
          receiver.revision() == locked_revision && receiver.selection_mask_handle() == selection && receiver.can_undo(),
          "failed remote save changed pixels, selection, revision or history");
    check(bool(lock->rollback()), "cannot release receiver lock");
    check(receiver.receive_edit(undo) && receiver.snapshot() == source.snapshot(),
          "undo was not transmitted as the same bounded edit");
    check(source.rename("paint-layer", "Shared paint"), "layer rename failed");
    const auto property = wire();
    check(change.impact && change.impact->layers == std::vector<std::string>{"paint-layer"} &&
          change.impact->pixels.empty() && property.size() < 4096,
          "layer rename did not produce a scoped event");
    const auto before_property = receiver.snapshot();
    check(receiver.receive_edit(property) && receiver.snapshot() == source.snapshot(),
          "layer property did not produce the same document");
    check(receiver.selection_mask_handle() == selection && receiver.selected() == receiver_selected,
          "remote layer property changed local selection");
    const auto after_property = receiver.snapshot();
    check(receiver.undo() && receiver.snapshot() == before_property &&
          receiver.redo() && receiver.snapshot() == after_property,
          "remote layer property did not preserve undo/redo");
    check(source.visible("paint-layer", false), "visibility change failed");
    const auto visibility = wire();
    auto property_lock = sql::Transaction::begin(*database, true);
    check(bool(property_lock), "cannot lock property working file");
    const auto before_failed_property = receiver.snapshot();
    const auto before_failed_revision = receiver.revision();
    check(!receiver.receive_edit(visibility) && receiver.snapshot() == before_failed_property &&
          receiver.revision() == before_failed_revision && receiver.selection_mask_handle() == selection,
          "failed property save changed the document or selection");
    check(bool(property_lock->rollback()), "cannot release property lock");
    const auto apply_property = [&] {
        check(receiver.receive_edit(wire()) && receiver.snapshot() == source.snapshot(),
              "layer property did not replicate exactly");
    };
    apply_property();
    check(source.opacity("paint-layer", .47), "opacity change failed"); apply_property();
    check(source.visible("paint-layer", true), "visibility restoration failed"); apply_property();
    check(source.transform(AffineTransform{1, 0, 0, 1, 4, 6}), "transform change failed"); apply_property();
    check(source.cycle_blend("paint-layer"), "blend change failed"); apply_property();

    Artwork largeSource(temp.path()), largeReceiver(temp.path());
    check(largeSource.create(8193,8193) && largeReceiver.receive(largeSource.snapshot()),
          "large tiled canvas setup failed");
    largeSource.document_changed = [&](const auto& value) { change = value; };
    check(largeSource.begin({250,250}, 1, brush) && largeSource.extend({280,280}, .6) &&
          largeSource.end({300,300}, .8), "cross-chunk stroke failed");
    check(change.impact && change.impact->pixels.size() == 1 && change.impact->assets.empty() &&
          change.impact->pixels.front().regions.size() >= 2, "cross-chunk stroke did not emit changed chunk regions");
    check(largeReceiver.receive_edit(wire()) && largeReceiver.snapshot() == largeSource.snapshot(),
          "cross-chunk stroke diverged");
    check(largeSource.undo() && largeReceiver.receive_edit(wire()) &&
          largeReceiver.snapshot() == largeSource.snapshot(), "cross-chunk undo did not remove the exact chunks");

}
}
int main() {
    try { run(); std::cout << "COLLABORATIVE_RASTER_EDIT_OK\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
