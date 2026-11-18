#pragma once

#include <iiSDKCore>
#include <memory>
#include <thread>

namespace congregation
{
enum class EngineState
{
    idle,
    running,
    stopped
};

// Owns or borrows the runtime event loop and controls application task lifetime.
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

  private:
    std::thread::id m_owner;
    EngineState m_state = EngineState::idle;
    std::unique_ptr<ii::runtime::EventLoop> m_ownedEvents;
    ii::runtime::EventLoop& m_events;
    ii::runtime::TaskScope m_tasks;
};
} // namespace congregation
