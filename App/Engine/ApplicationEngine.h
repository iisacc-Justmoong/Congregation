#pragma once

#include "CanvasDocument.h"

#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiSDKCore/Runtime/TaskScope.h>
#include <memory>

namespace congregation
{
enum class EngineState
{
    idle,
    running,
    stopped
};

// Windowing and QML are presentation-host responsibilities. This engine also
// runs without them, using the same Core dispatch and editing contracts.
class ApplicationEngine final
{
  public:
    explicit ApplicationEngine(std::size_t capacity = 1024);
    explicit ApplicationEngine(ii::runtime::EventLoop& hostEvents);
    ~ApplicationEngine();
    [[nodiscard]] ii::runtime::Submission start(ii::runtime::Task onStarted);
    void stop();
    [[nodiscard]] EngineState state() const noexcept { return m_state; }
    [[nodiscard]] ii::runtime::EventLoop& events() noexcept { return m_events; }
    [[nodiscard]] ii::runtime::TaskScope& tasks() noexcept { return m_tasks; }
    [[nodiscard]] CanvasDocument& document() noexcept { return m_document; }

  private:
    std::thread::id m_owner;
    EngineState m_state = EngineState::idle;
    std::unique_ptr<ii::runtime::EventLoop> m_ownedEvents;
    ii::runtime::EventLoop& m_events;
    CanvasDocument m_document;
    ii::runtime::TaskScope m_tasks;
};
} // namespace congregation
