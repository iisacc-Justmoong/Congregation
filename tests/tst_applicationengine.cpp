#include <Engine/ApplicationEngine.h>
#include <Engine/Palette.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace
{
void check(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void canvasContracts()
{
    using namespace congregation;
    CanvasDocument document;
    auto &queries = document.queries();
    auto send = [&](CanvasQuery query) { return queries.send(query).value(); };
    const auto initial = send(ReadCanvas{});
    check(initial.brushSize == 2.0 && initial.canvasWidth == 1, "canvas defaults");
    check(send(SetReal{RealProperty::brushSize, 90}).brushSize == 48, "size upper bound");
    check(send(SetReal{RealProperty::brushSize, -1}).brushSize == 1, "size lower bound");
    const auto beforeInvalid = document.snapshot();
    for (double value : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()})
    {
        check(send(SetReal{RealProperty::brushFlow, value}) == beforeInvalid,
              "non-finite input must not mutate the document");
    }
    send(SetReal{RealProperty::pressureMinimum, 0.7});
    check(document.snapshot().pressureCenter == 0.7, "minimum raises center");
    auto pressure = send(SetReal{RealProperty::pressureMaximum, 0.4});
    check(pressure.pressureMinimum == 0.4 && pressure.pressureCenter == 0.4,
          "maximum preserves ordered pressure curve");
    check(send(SetReal{RealProperty::pressureCenter, -2}).pressureCenter == 0.4,
          "center remains inside endpoints");
    check(send(SetTool{"eraser"}).tool == "eraser", "supported tool");
    check(send(SetTool{"translate"}).tool == "brush", "unsupported tool normalization");
    check(send(SetShape{"  TRIAGLE  "}).shape == "triangle", "legacy shape alias");
    check(send(SetShape{"unknown"}).shape == "rectangle", "unsupported shape");
    check(send(SetDimension{Dimension::width, 0}).canvasWidth == 1, "positive width");
    check(send(SetDimension{Dimension::height, -4}).canvasHeight == 1, "positive height");
    const auto beforeRead = document.snapshot();
    check(send(ReadCanvas{}) == beforeRead, "read is mutation-free");
    check(send(SetTool{"brush"}) == beforeRead, "identical edits preserve revision");

    ii::runtime::QueryReply<CanvasSnapshot> foreign;
    std::thread worker([&] { foreign = queries.send(SetTool{"fill"}); });
    worker.join();
    check(!foreign && foreign.error().code == ii::runtime::QueryErrorCode::handler_failed,
          "foreign direct mutation must fail");
    check(document.snapshot() == beforeRead, "foreign mutation preserves owner state");

    ii::runtime::EventLoop events;
    bool delivered = false;
    auto queued = queries.post(events, SetTool{"fill"}, [&](auto reply) {
        delivered = reply && reply->tool == "fill";
    });
    check(bool(queued) && !delivered, "query admission is asynchronous");
    events.poll();
    check(delivered, "query dispatches on document owner");
    bool removedReceiverCalled = false;
    {
        CanvasDocument temporary;
        check(bool(temporary.queries().post(events, ReadCanvas{}, [&](auto) {
            removedReceiverCalled = true;
        })), "temporary receiver admission");
    }
    events.poll();
    check(!removedReceiverCalled, "removed receiver suppresses queued completion");
}

void paletteContracts()
{
    using namespace congregation;
    const std::vector<PaletteEntry> primary{
        {"white", {1, 1, 1, 1}}, {"red", {1, 0, 0, 1}},
        {"black", {0, 0, 0, 1}}, {"invalid", {0, 0, 0, 1, false}}};
    const std::vector<PaletteEntry> extended{
        {"duplicate", {1, 0, 0, 0.5}}, {"blue", {0, 0, 1, 1}}};
    const auto result = buildPalette(primary, extended);
    check(result.size() == 4, "invalid colors and RGB duplicates omitted");
    check(result[0].name == "black" && result[1].name == "red" &&
          result[2].name == "blue" && result[3].name == "white", "palette order");
    check(defaultPalette().size() == 19, "complete default palette");
}

void engineContracts()
{
    using namespace congregation;
    ApplicationEngine engine(8);
    int launches = 0;
    check(bool(engine.start([&] { ++launches; })), "engine starts");
    check(!engine.start([&] { ++launches; }), "engine refuses a second start");
    check(launches == 0, "startup waits for host dispatch");
    engine.events().poll();
    check(launches == 1, "startup runs exactly once");
    bool late = false;
    check(bool(engine.tasks().at(engine.events(), ii::runtime::Clock::now() + std::chrono::hours(1),
                                [&] { late = true; })),
          "delayed task admission");
    engine.stop();
    engine.events().poll();
    check(!late && engine.state() == EngineState::stopped, "shutdown cancels pending callbacks");
    check(!engine.events().post([] {}), "shutdown closes admission");
    check(!engine.start([] {}), "stopped engine is not restarted");
    engine.stop();

    ApplicationEngine bounded(1);
    auto occupied = bounded.events().post([] {});
    check(bool(occupied), "fill bounded queue");
    check(!bounded.start([] {}) && bounded.state() == EngineState::idle,
          "rejected startup leaves engine idle");
    check(bounded.events().cancel(occupied.token), "cancel occupied slot");
    check(bool(bounded.start([] {})), "retry rejected startup");
}
void borrowedEngineContracts()
{
    ii::runtime::EventLoop host;
    bool called=false,retired=false,hostAlive=false;
    {
        congregation::ApplicationEngine engine(host);
        check(&engine.events()==&host,"native application must share the Core event loop");
        check(bool(engine.start([&]{called=true;})),"borrowed engine startup");
        host.poll();
        check(called,"borrowed loop dispatch");
        check(bool(engine.tasks().at(host,ii::runtime::Clock::now()+std::chrono::hours(1),[&]{retired=true;})),"borrowed task admission");
        engine.stop();
    }
    check(bool(host.post([&]{hostAlive=true;})),"engine shutdown must preserve the native host loop");
    host.poll();
    check(hostAlive&&!retired,"engine retirement must cancel only its own work");
}
} // namespace

int main()
{
    try
    {
        canvasContracts();
        paletteContracts();
        engineContracts();
        borrowedEngineContracts();
        std::cout << "PASS: canvas, typed queries, palette, startup, bounds and shutdown\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
