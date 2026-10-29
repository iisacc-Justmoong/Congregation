#include <Engine/CanvasSession.h>
#include <Engine/RecentCanvas.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace congregation;
using namespace std::chrono_literals;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class F> bool wait(ii::runtime::EventLoop& loop, F condition, std::chrono::milliseconds time = 1500ms) {
    const auto deadline = ii::runtime::Clock::now() + time;
    while (!condition() && ii::runtime::Clock::now() < deadline) {
        loop.poll();
        std::this_thread::sleep_for(1ms);
    }
    return condition();
}
std::string snapshot(char fill) {
    RecentCanvas canvas;
    canvas.document = std::string(8192, fill);
    auto encoded = encode_canvas(canvas);
    check(bool(encoded), "could not encode snapshot");
    return std::move(*encoded);
}
void run(const std::string& mode) {
    ii::runtime::EventLoop loop;
    int rejects = 0, calls = 0, rejected = 0;
    bool rejected_with_empty_queue = false;
    ii::network::Tcp* host_socket = nullptr;
    auto writer = [&](ii::network::Tcp& socket, std::shared_ptr<const std::string> bytes) {
        host_socket = &socket;
        ++calls;
        if (rejects > 0) {
            --rejects;
            ++rejected;
            rejected_with_empty_queue |= socket.pending_bytes() == 0;
            if (mode == "throw") throw std::bad_alloc();
            return false;
        }
        return socket.write(std::move(bytes));
    };
    auto host = std::make_unique<CanvasSession>(loop, uuid(), CanvasSession::default_peer_pending_bytes, writer);
    CanvasSession client(loop, uuid());
    std::string last;
    client.snapshot_received = [&](std::string value, std::uint64_t, std::string) { last = std::move(value); };
    const auto initial = snapshot('a'), first = snapshot('b'), newest = snapshot('c');
    check(host->host("Host") && host->publish(initial), "host setup failed");
    check(client.join(host->session, "127.0.0.1", host->port(), "Guest"), "join failed");
    check(wait(loop, [&] { return client.revision == 1 && last == initial && host_socket && host_socket->pending_bytes() == 0; }),
          "initial snapshot did not drain");
    std::vector<ii::runtime::TaskToken> occupied;
    int error_notices = 0;
    bool stopped_in_notice = false, rehost_ok = false;
    if (mode == "scheduler_full" || mode == "reentrant_stop") {
        host->changed = [&](CanvasSession::Notice notice) {
            if (notice == CanvasSession::Notice::error && !host->error.empty()) {
                ++error_notices;
                if (mode == "reentrant_stop") { stopped_in_notice = true; host->stop(); }
            }
        };
        bool full = false;
        for (int i = 0; i < 4096; ++i) {
            auto submitted = loop.after(60s, [] {});
            if (!submitted) { full = true; break; }
            occupied.push_back(submitted.token);
        }
        check(full && !occupied.empty(), "could not fill event-loop task slots");
    }
    if (mode == "revision_stop" || mode == "revision_rehost") {
        host->changed = [&](CanvasSession::Notice notice) {
            if (notice == CanvasSession::Notice::revision && !stopped_in_notice) {
                stopped_in_notice = true;
                host->stop();
                if (mode == "revision_rehost") rehost_ok = host->host("Replacement host");
            }
        };
    }
    rejects = mode == "revision_stop" || mode == "revision_rehost" ? 0 :
              mode == "repeated" ? 3 : mode == "coalesce" ? 2 : 1;
    bool first_published = false, threw = false;
    try { first_published = host->publish(first); } catch (const std::bad_alloc&) { threw = true; }
    if (threw) {
        std::cerr << "EMPTY_QUEUE_RETRY_THREW mode=" << mode << '\n';
        throw std::runtime_error("snapshot allocation failure escaped");
    }
    const bool reentrant = mode == "reentrant_stop" || mode == "revision_stop" || mode == "revision_rehost";
    check(first_published != reentrant, "publication result did not reflect reentrant session change");
    if (mode == "coalesce") check(host->publish(newest), "second publish failed");
    if (mode != "revision_stop" && mode != "revision_rehost")
        check(rejected_with_empty_queue, "injection did not reject with an empty queue");
    if (mode == "reentrant_stop") {
        for (const auto& token : occupied) token.cancel();
        check(stopped_in_notice && host->state == CanvasSession::State::idle,
              "error callback did not stop the session");
        const auto deadline = ii::runtime::Clock::now() + 100ms;
        while (ii::runtime::Clock::now() < deadline) { loop.poll(); std::this_thread::sleep_for(1ms); }
    } else if (mode == "revision_stop" || mode == "revision_rehost") {
        check(stopped_in_notice && (mode == "revision_rehost" ? rehost_ok && host->state == CanvasSession::State::hosting
                                                               : host->state == CanvasSession::State::idle),
              "revision callback did not produce the expected session change");
        host->stop();
    } else if (mode == "scheduler_full") {
        check(host->error == "A canvas update could not be scheduled for retry." && error_notices > 0,
              "retry scheduling failure was not reported");
        for (const auto& token : occupied) token.cancel();
        check(host->publish(newest), "publish after freeing task slots failed");
        check(wait(loop, [&] { return client.revision == 3 && last == newest; }),
              "publishing did not recover after freeing task slots");
        host->stop();
    } else if (mode == "stop") {
        host.reset();
        const int calls_at_stop = calls;
        const auto deadline = ii::runtime::Clock::now() + 250ms;
        while (ii::runtime::Clock::now() < deadline) { loop.poll(); std::this_thread::sleep_for(1ms); }
        check(calls == calls_at_stop, "retry ran after host destruction");
    } else {
        const auto wanted_revision = mode == "coalesce" ? 3u : 2u;
        const auto& wanted = mode == "coalesce" ? newest : first;
        if (!wait(loop, [&] { return client.revision == wanted_revision && last == wanted; })) {
            std::cerr << "EMPTY_QUEUE_RETRY_LOST mode=" << mode << " host=" << host->revision
                      << " client=" << client.revision << " rejected=" << rejected << '\n';
            throw std::runtime_error("rejected empty-queue snapshot was not retried");
        }
        check(rejected == (mode == "repeated" ? 3 : mode == "coalesce" ? 2 : 1),
              "unexpected reject count");
        host->stop();
    }
    client.stop();
    std::cout << "EMPTY_QUEUE_RETRY_OK " << mode << '\n';
}
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected case");
        const std::string mode = argv[1];
        check(mode == "once" || mode == "repeated" || mode == "coalesce" || mode == "stop" || mode == "throw" || mode == "scheduler_full" || mode == "reentrant_stop" || mode == "revision_stop" || mode == "revision_rehost", "unknown case");
        run(mode);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
