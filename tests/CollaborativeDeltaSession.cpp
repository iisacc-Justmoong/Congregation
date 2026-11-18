#include <Collaboration/CanvasSession.h>
#include <Collaboration/LocalChangeQueue.h>
#include <Document/RecentCanvas.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace congregation;
using namespace std::chrono_literals;

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> bool wait(ii::runtime::EventLoop& loop, F predicate) {
    const auto deadline = ii::runtime::Clock::now() + 10s;
    while (!predicate() && ii::runtime::Clock::now() < deadline) {
        loop.poll();
        std::this_thread::sleep_for(1ms);
    }
    return predicate();
}
std::string snapshot(char value) {
    RecentCanvas canvas;
    canvas.document = std::string(1024, value);
    auto encoded = encode_canvas(canvas);
    check(bool(encoded), "cannot encode fixture");
    return std::move(*encoded);
}
void ordered_changes() {
    ii::runtime::EventLoop loop;
    int reject_writes = 0;
    auto writer = [&](ii::network::Tcp& socket, std::shared_ptr<const std::string> bytes) {
        if (reject_writes > 0) { --reject_writes; return false; }
        return socket.write(std::move(bytes));
    };
    CanvasSession host(loop, uuid(), CanvasSession::default_peer_pending_bytes, writer);
    CanvasSession first(loop, uuid()), second(loop, uuid());
    const auto initial = snapshot('a'), current = snapshot('b');
    check(host.host("host") && host.initialize(initial), "host setup failed");
    std::vector<std::string> first_changes, second_changes;
    std::string first_snapshot, second_snapshot;
    first.snapshot_received = [&](std::string bytes, auto, auto) { first_snapshot = std::move(bytes); };
    first.delta_received = [&](std::string_view bytes, auto, auto) {
        first_changes.emplace_back(bytes); return true;
    };
    second.snapshot_received = [&](std::string bytes, auto, auto) { second_snapshot = std::move(bytes); };
    second.delta_received = [&](std::string_view bytes, auto, auto) {
        second_changes.emplace_back(bytes); return true;
    };
    check(first.join(host.session, "127.0.0.1", host.port(), "first"), "first join failed");
    check(wait(loop, [&] { return first.revision == 1 && first_snapshot == initial; }), "initial snapshot missing");
    host.snapshot_provider = [&] { return current; };
    reject_writes = 2;
    first.set_profile("updated first");
    check(host.publish_delta("one") && host.publish_delta("two") && host.publish_delta("three"),
          "host deltas rejected");
    check(wait(loop, [&] { return first.revision == 4 && first_changes.size() == 3; }),
          "queued deltas did not arrive");
    check(first_changes == std::vector<std::string>({"one", "two", "three"}),
          "a pending incremental edit was coalesced or reordered");
    check(second.join(host.session, "127.0.0.1", host.port(), "second"), "second join failed");
    check(wait(loop, [&] { return second.revision == 4 && second_snapshot == current; }),
          "late participant did not receive a fresh checkpoint at the current revision");
    check(first_changes.size() == 3 && host.revision == 4,
          "late participant checkpoint changed the shared revision");
    std::string requested, sender;
    host.delta_requested = [&](std::string_view bytes, std::string_view peer) {
        requested = bytes; sender = peer; return true;
    };
    check(first.submit_delta("guest", first.revision), "guest delta was not submitted");
    check(wait(loop, [&] { return first.revision == 5 && second.revision == 5; }),
          "guest change was not broadcast");
    check(requested == "guest" && sender == first.peer_id && first_changes.back() == "guest" &&
          second_changes == std::vector<std::string>({"guest"}), "guest delta was not delivered exactly once");
    check(first.submit_delta("stale", 1), "stale delta could not be sent for validation");
    check(wait(loop, [&] { return first.state == CanvasSession::State::idle; }), "stale sender was not disconnected");
    check(host.revision == 5 && requested == "guest", "stale delta changed host state");
    first.stop(); second.stop(); host.stop(); loop.poll();
}
void optimistic_queue() {
    LocalChangeQueue queue(64);
    check(queue.push("one") &&
          queue.push("two"), "cannot queue local changes");
    check(queue.ready() && queue.front() == "one" && queue.mark_sent() && !queue.ready(),
          "more than one change could be in flight");
    check(!queue.acknowledge("wrong") && queue.size() == 2,
          "incorrect acknowledgement discarded local edits");
    check(queue.acknowledge("one") && queue.ready() &&
          queue.front() == "two" && queue.mark_sent(),
          "acknowledgement did not release the next change");
    check(queue.acknowledge("two") && queue.empty(),
          "edit acknowledgement did not drain the queue");
    check(!queue.push(std::string(65, 'x')) && queue.empty(),
          "queue admitted an edit above its budget");
}
void unavailable_retry_disconnects() {
    ii::runtime::EventLoop loop;
    bool reject_writes = false;
    auto writer = [&](ii::network::Tcp& socket, std::shared_ptr<const std::string> bytes) {
        return !reject_writes && socket.write(std::move(bytes));
    };
    CanvasSession host(loop, uuid(), CanvasSession::default_peer_pending_bytes, writer);
    CanvasSession guest(loop, uuid());
    const auto initial = snapshot('r');
    check(host.host("host") && host.initialize(initial), "retry fixture host failed");
    check(guest.join(host.session, "127.0.0.1", host.port(), "guest"), "retry fixture join failed");
    check(wait(loop, [&] { return guest.revision == 1; }), "retry fixture checkpoint missing");
    std::vector<ii::runtime::TaskToken> occupied;
    bool full = false;
    for (int i = 0; i < 4096; ++i) {
        auto submitted = loop.after(60s, [] {});
        if (!submitted) { full = true; break; }
        occupied.push_back(submitted.token);
    }
    check(full, "could not exhaust the retry scheduler");
    reject_writes = true;
    check(host.publish_delta("must-not-be-lost"), "delta publication failed unexpectedly");
    for (const auto& token : occupied) token.cancel();
    check(wait(loop, [&] { return guest.state == CanvasSession::State::idle; }),
          "a participant was left connected with an undeliverable delta");
    guest.stop(); host.stop(); loop.poll();
}
void failed_checkpoint_does_not_advance() {
    ii::runtime::EventLoop loop;
    CanvasSession host(loop, uuid()), guest(loop, uuid());
    const auto initial = snapshot('a');
    check(host.host("host") && host.initialize(initial), "checkpoint host failed");
    check(guest.join(host.session, "127.0.0.1", host.port(), "guest"), "checkpoint guest failed");
    guest.snapshot_applying = [](std::string_view, std::uint64_t, std::string_view) { return false; };
    check(!host.initialize(snapshot('b')), "a host can initialize only once");
    check(wait(loop, [&] { return guest.state == CanvasSession::State::idle; }) && guest.revision == 0,
          "failed initialization advanced the collaboration revision");
    guest.stop(); host.stop(); loop.poll();
}
}
int main() {
    try { optimistic_queue(); ordered_changes(); unavailable_retry_disconnects(); failed_checkpoint_does_not_advance(); std::cout << "COLLABORATIVE_DELTA_SESSION_OK\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
