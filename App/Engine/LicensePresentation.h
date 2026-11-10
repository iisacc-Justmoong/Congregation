#pragma once

namespace congregation {
// Workspace visibility is independent of authorization for protected commands.
constexpr bool license_workspace_visible(bool enforced, bool licensed,
                                         bool initial_verification_pending) noexcept {
    return !enforced || licensed || initial_verification_pending;
}
}
