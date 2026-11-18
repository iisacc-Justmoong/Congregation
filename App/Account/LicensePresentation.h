#pragma once

namespace congregation {
// Workspace visibility is independent of authorization for protected commands.
constexpr bool license_workspace_visible(bool enforced, bool licensed,
                                         bool initial_verification_pending) noexcept {
    return !enforced || licensed || initial_verification_pending;
}
// Local edits remain provisional while the first decision is pending. A visible
// workspace does not constitute a verified entitlement for remote services.
constexpr bool license_local_edits_allowed(bool enforced, bool licensed,
                                           bool initial_verification_pending) noexcept {
    return license_workspace_visible(enforced, licensed, initial_verification_pending);
}
constexpr bool license_defer_rejection(bool visible, bool interaction_active,
                                       bool first_presentation_pending = false) noexcept {
    return !visible && (interaction_active || first_presentation_pending);
}
}
