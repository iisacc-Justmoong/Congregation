#include <Engine/CanvasSession.h>
#include <Engine/RecentCanvas.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

using namespace congregation;
using namespace std::chrono_literals;
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Predicate> bool wait(ii::runtime::EventLoop& loop, Predicate condition) {
    const auto end = ii::runtime::Clock::now() + 5s;
    while (!condition() && ii::runtime::Clock::now() < end) {
        loop.poll();
        std::this_thread::sleep_for(1ms);
    }
    return condition();
}
std::string snapshot(char value) {
    RecentCanvas canvas;
    canvas.document = std::string(8192, value);
    auto encoded = encode_canvas(canvas);
    check(bool(encoded), "failed to encode fixture");
    return std::move(*encoded);
}
void run(const std::string& mode) {
    ii::runtime::EventLoop loop;
    CanvasSession* owner = nullptr;
    ii::network::Tcp* hostSocket = nullptr;
    bool armed = false, nestedReject = false, callbackRan = false, nestedPublished = false;
    const auto initial = snapshot('a'), first = snapshot('b'), newest = snapshot('c');
    auto writer = [&](ii::network::Tcp& socket, std::shared_ptr<const std::string> bytes) {
        hostSocket = &socket;
        if (armed) {
            armed = false;
            callbackRan = true;
            if (mode == "publish") {
                nestedReject = true;
                nestedPublished = owner->publish(newest);
                return socket.write(std::move(bytes));
            }
            if (mode == "throw_runtime") throw std::runtime_error("writer failed");
            owner->stop();
            if (mode == "rehost") {
                nestedPublished = owner->host("Replacement host");
                return true;
            }
            if (mode == "stop_throw") throw std::bad_alloc();
            return mode == "stop_true";
        }
        if (nestedReject) {
            nestedReject = false;
            return false;
        }
        return socket.write(std::move(bytes));
    };
    CanvasSession host(loop, uuid(), CanvasSession::default_peer_pending_bytes, writer);
    owner = &host;
    CanvasSession guest(loop, uuid());
    std::string received;
    guest.snapshot_received = [&](std::string value, std::uint64_t, std::string) {
        received = std::move(value);
    };
    check(host.host("Host") && host.publish(initial), "host setup failed");
    check(guest.join(host.session, "127.0.0.1", host.port(), "Guest"), "guest setup failed");
    check(wait(loop, [&] { return guest.revision == 1 && received == initial && hostSocket &&
                                 hostSocket->pending_bytes() == 0; }), "initial update did not drain");
    armed = true;
    bool accepted = false, threw = false;
    try { accepted = host.publish(first); }
    catch (const std::runtime_error&) { threw = true; }
    check(callbackRan, "the writer callback was not invoked");
    if (mode == "throw_runtime") {
        check(threw && host.state == CanvasSession::State::hosting,
              "the writer exception was not propagated");
        check(host.publish(newest), "the next publication could not proceed after an exception");
        check(wait(loop, [&] { return guest.revision == 3 && received == newest; }),
              "the latest update was not sent after a writer exception");
        host.stop();
    } else if (mode == "publish") {
        check(!threw, "publication unexpectedly threw");
        check(accepted && nestedPublished, "nested publication was rejected");
        if (!wait(loop, [&] { return guest.revision == 3 && received == newest; })) {
            std::cerr << "WRITER_REENTRY_LOST host=" << host.revision
                      << " guest=" << guest.revision << '\n';
            throw std::runtime_error("a newer pending update was lost");
        }
        host.stop();
    } else if (mode == "rehost") {
        check(!threw && !accepted && nestedPublished && host.state == CanvasSession::State::hosting,
              "an old publication survived rehosting");
        host.stop();
    } else {
        check(!threw && !accepted && host.state == CanvasSession::State::idle,
              "publication survived a reentrant stop");
    }
    guest.stop();
    std::cout << "WRITER_REENTRY_OK " << mode << '\n';
}
} // namespace
int main(int argc, char** argv) {
    try {
        check(argc == 2, "missing case");
        const std::string mode = argv[1];
        check(mode == "stop_true" || mode == "stop_false" || mode == "stop_throw" || mode == "publish" ||
                  mode == "throw_runtime" || mode == "rehost",
              "unknown case");
        run(mode);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
