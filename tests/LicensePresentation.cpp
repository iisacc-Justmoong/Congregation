#include "../App/Engine/LicensePresentation.h"
#include <iostream>

int main() {
    using congregation::license_workspace_visible;
    const bool valid = license_workspace_visible(true, false, true)
        && !license_workspace_visible(true, false, false)
        && license_workspace_visible(true, true, false)
        && license_workspace_visible(false, false, false);
    if (!valid) std::cerr << "License workspace visibility contract failed\n";
    return valid ? 0 : 1;
}
