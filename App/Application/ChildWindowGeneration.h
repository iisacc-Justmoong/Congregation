#pragma once

#include <memory>

namespace congregation::native
{
// A child window's late callbacks may outlive that particular opening.
// Tickets do not own the window and cannot match a subsequent opening.
class ChildWindowGeneration final
{
  public:
    struct Ticket final { std::weak_ptr<void> identity; };

    [[nodiscard]] Ticket begin()
    {
        current_ = std::make_shared<char>();
        closing_ = false;
        return {current_};
    }
    [[nodiscard]] bool owns(const Ticket& ticket) const noexcept
    {
        auto identity = ticket.identity.lock();
        return identity && identity == current_;
    }
    [[nodiscard]] bool accepts(const Ticket& ticket) const noexcept
    {
        return !closing_ && owns(ticket);
    }
    bool close(const Ticket& ticket) noexcept
    {
        if (!accepts(ticket)) return false;
        closing_ = true;
        return true;
    }
    bool retire(const Ticket& ticket) noexcept
    {
        if (!owns(ticket)) return false;
        closing_ = true;
        current_.reset();
        return true;
    }

  private:
    std::shared_ptr<void> current_;
    bool closing_ = true;
};
} // namespace congregation::native
