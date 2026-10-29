#include <Native/Artwork.h>
#include <iiFileProviderNative.h>
#include <iiFileProviderDatabase.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

using congregation::native::Artwork;
namespace sql = iiFileProvider::native::sql;
namespace {
void check(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}
void selectionIs(const Artwork& artwork, const std::string& expected, std::string_view when) {
    if (artwork.selected() != expected) {
        std::cerr << "ROLLBACK_SELECTION_MISMATCH " << when << " expected=" << expected
                  << " actual=" << artwork.selected() << '\n';
        throw std::runtime_error("selection was not restored");
    }
}
}
int main(int argc, char** argv) {
    try {
        check(argc==3, "expected resource directory and case");
        const std::string mode=argv[2];
        iiFileProvider::native::TemporaryDirectory temp{std::filesystem::canonical(argv[1])};
        check(temp.isValid(), "cannot create private test directory");
        std::cout << "test-resource-dir=" << temp.path().string() << '\n' << std::flush;
        Artwork local(temp.path());
        check(local.create(32,32), local.error());
        check(local.add_layer("Local"), local.error());
        const auto localSelection=local.selected();
        Artwork remote(temp.path());
        check(remote.create(32,32), remote.error());
        check(remote.add_layer("Remote"), remote.error());
        const auto remoteSelection=remote.selected();
        check(localSelection!=remoteSelection, "fixtures unexpectedly share selection ID");
        const auto received=remote.snapshot();
        check(!received.empty(), "cannot encode remote fixture");
        if (mode=="receive_same") {
            check(local.select("paint-layer"), local.error());
            const auto oldSelection=local.selected();
            const auto before=local.snapshot();
            const auto revision=local.revision();
            const bool undo=local.can_undo(), redo=local.can_redo();
            int changed=0, selected=0;
            local.changed=[&] { ++changed; };
            local.selection_changed=[&] { ++selected; };
            check(local.receive(before), local.error());
            check(local.snapshot()==before && local.revision()==revision &&
                  local.can_undo()==undo && local.can_redo()==redo,
                  "unchanged receive modified the document or history");
            selectionIs(local,oldSelection,"unchanged receive");
            check(changed==0 && selected==0,"unchanged receive published notifications");
            std::cout << "SELECTION_CASE_OK " << mode << '\n';
            return 0;
        }
        if (mode=="receive_undo") {
            const auto before=local.snapshot();
            check(local.receive(received), local.error());
            selectionIs(local,remoteSelection,"receive");
            check(local.undo(), local.error());
            check(local.snapshot()==before,"undo did not restore local document");
            selectionIs(local,localSelection,"undo receive");
            check(local.redo(), local.error());
            selectionIs(local,remoteSelection,"redo receive");
            std::cout << "SELECTION_CASE_OK " << mode << '\n';
            return 0;
        }
        if (mode=="redo_lock") check(local.undo(), local.error());
        const auto oldSelection=local.selected();
        const auto before=local.snapshot();
        const auto revision=local.revision();
        const bool undo=local.can_undo(), redo=local.can_redo();
        int changed=0, selected=0;
        local.changed=[&] { ++changed; };
        local.selection_changed=[&] { ++selected; };
        auto database=sql::Database::open(local.recovery());
        check(bool(database), "cannot open recovery file for locking");
        auto lock=sql::Transaction::begin(*database,true);
        check(bool(lock), "cannot acquire write lock");
        bool accepted=false;
        if (mode=="add_lock") accepted=local.add_layer("Blocked");
        else if (mode=="receive_lock") accepted=local.receive(received);
        else if (mode=="undo_lock") accepted=local.undo();
        else if (mode=="redo_lock") accepted=local.redo();
        else throw std::runtime_error("unknown test case");
        check(!accepted, "locked operation unexpectedly succeeded");
        check(local.snapshot()==before && local.revision()==revision &&
              local.can_undo()==undo && local.can_redo()==redo,
              "failed write changed the document or history");
        selectionIs(local,oldSelection,"failed write");
        check(changed==0 && selected==0,"failed write published change notifications");
        check(lock->rollback().has_value(), "cannot release lock");
        if (mode=="add_lock") {
            check(local.add_layer("Retried"), local.error());
            check(local.undo(), local.error());
            selectionIs(local,oldSelection,"undo retried add");
        } else if (mode=="receive_lock") {
            check(local.receive(received), local.error());
            check(local.undo(), local.error());
            selectionIs(local,oldSelection,"undo retried receive");
        } else if (mode=="undo_lock") {
            check(local.undo(), local.error());
            check(local.redo(), local.error());
            selectionIs(local,oldSelection,"redo retried undo");
        } else {
            check(local.redo(), local.error());
            check(local.undo(), local.error());
            selectionIs(local,oldSelection,"undo retried redo");
        }
        std::cout << "SELECTION_CASE_OK " << mode << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
