#pragma once
#include <utility>
namespace congregation
{
template <class F> class ScopeExit final
{
    F callback_;

  public:
    explicit ScopeExit(F callback) : callback_(std::move(callback)) {}
    ~ScopeExit() { callback_(); }
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
};
} // namespace congregation
