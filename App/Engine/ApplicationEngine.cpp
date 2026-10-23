#include "ApplicationEngine.h"

#include <stdexcept>
#include <utility>

namespace congregation
{
ApplicationEngine::ApplicationEngine(std::size_t capacity)
    : m_owner(std::this_thread::get_id()),
      m_ownedEvents(std::make_unique<ii::runtime::EventLoop>(capacity)), m_events(*m_ownedEvents)
{
}

ApplicationEngine::ApplicationEngine(ii::runtime::EventLoop& hostEvents)
    : m_owner(std::this_thread::get_id()), m_events(hostEvents)
{
}

ApplicationEngine::~ApplicationEngine()
{
    stop();
}

ii::runtime::Submission ApplicationEngine::start(ii::runtime::Task onStarted)
{
    if (std::this_thread::get_id() != m_owner)
    {
        throw std::logic_error("ApplicationEngine must start on its host thread");
    }
    if (m_state != EngineState::idle)
    {
        return {{}, ii::runtime::SubmitError::stopped};
    }
    auto accepted = m_tasks.post(m_events, std::move(onStarted));
    if (accepted)
    {
        m_state = EngineState::running;
    }
    return accepted;
}

void ApplicationEngine::stop()
{
    if (std::this_thread::get_id() != m_owner)
    {
        throw std::logic_error("ApplicationEngine must stop on its host thread");
    }
    if (m_state == EngineState::stopped)
    {
        return;
    }
    if (!m_tasks.close())
    {
        throw std::logic_error("ApplicationEngine cannot stop inside its own callback");
    }
    if (m_ownedEvents) m_events.request_stop();
    m_state = EngineState::stopped;
}
} // namespace congregation
