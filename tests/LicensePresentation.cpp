#include "../App/Engine/LicensePresentation.h"
#include <iostream>

int main() {
    using congregation::license_workspace_visible;
    const bool valid = license_workspace_visible(true, false, true)
        && !license_workspace_visible(true, false, false)
        && license_workspace_visible(true, true, false)
        && license_workspace_visible(false, false, false)
        && congregation::license_local_edits_allowed(true, false, true)
        && !congregation::license_local_edits_allowed(true, false, false)
        && congregation::license_local_edits_allowed(true, true, false)
        && congregation::license_defer_rejection(false, true)
        && congregation::license_defer_rejection(false, false, true)
        && !congregation::license_defer_rejection(false, false)
        && !congregation::license_defer_rejection(true, true)
        && !congregation::license_defer_rejection(true, false, true);
    if (!valid) std::cerr << "License workspace visibility contract failed\n";
    return valid ? 0 : 1;
}
