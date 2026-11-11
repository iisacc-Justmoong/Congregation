#pragma once
#include <iiNetwork/Wire.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace congregation::native {
namespace fs = std::filesystem;
struct SocietyAssetList final { fs::path root; std::vector<fs::path> files; std::string error; };
inline std::optional<ii::core::Value> read_society_json(const fs::path& path) {
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    if (ec || size > 1024 * 1024) return {};
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string contents(static_cast<std::size_t>(size), '\0');
    if (!f.read(contents.data(), static_cast<std::streamsize>(contents.size()))) return {};
    return ii::network::wire::parse_json(contents, 1024 * 1024);
}
inline fs::path society_registry() {
    if (const char* value = std::getenv("SOCIETY_STORAGE_SETTINGS_PATH"); value && *value)
        return fs::path(value).is_absolute() ? fs::path(value) : fs::path{};
#ifdef _WIN32
    const char* config = std::getenv("APPDATA");
    return config ? fs::path(config) / "iisacc/Society/storage.json" : fs::path{};
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home ? fs::path(home) / "Library/Preferences/iisacc/Society/storage.json" : fs::path{};
#else
    if (const char* config = std::getenv("XDG_CONFIG_HOME"); config && *config)
        return fs::path(config) / "iisacc/Society/storage.json";
    const char* home = std::getenv("HOME");
    return home ? fs::path(home) / ".config/iisacc/Society/storage.json" : fs::path{};
#endif
}
inline SocietyAssetList society_assets(const fs::path& registry, const fs::path& override_root = {}) {
    using namespace ii::network::wire;
    SocietyAssetList result;
    if (!override_root.empty()) result.root = override_root;
    else {
        const auto config = read_society_json(registry);
        if (!config) { result.error = "Society storage is not connected"; return result; }
        auto root = fs::path(text(field(*config, "path")));
        auto identifier = text(field(*config, "containerId"));
        const auto manifest = root.is_absolute() ? read_society_json(root / ".society-drive.json") : std::nullopt;
        if (!manifest || identifier.empty() || identifier != text(field(*manifest, "identifier"))) {
            result.error = "Society storage is unavailable"; return result;
        }
        for (const auto& section : array(field(*manifest, "sections"))) {
            if (text(field(section, "id")) != "asset-library") continue;
            fs::path relative = text(field(section, "path"));
            if (relative.empty() || relative.is_absolute()) break;
            bool safe = true;
            for (const auto& part : relative) if (part == ".." || part == ".") safe = false;
            if (!safe) break;
            result.root = root / relative;
            break;
        }
    }
    std::error_code ec;
    if (result.root.empty() || !result.root.is_absolute() || !fs::is_directory(result.root, ec) || ec) {
        result.error = "Asset Library is unavailable"; result.root.clear(); return result;
    }
    fs::recursive_directory_iterator it(result.root, fs::directory_options::skip_permission_denied, ec), end;
    while (!ec && it != end && result.files.size() < 512) {
        auto status = it->symlink_status(ec);
        if (ec) break;
        if (fs::is_regular_file(status) && !it->path().filename().string().starts_with('.'))
            result.files.push_back(it->path());
        it.increment(ec);
    }
    if (ec) result.error = "Unable to read all assets";
    std::ranges::sort(result.files);
    return result;
}
} // namespace congregation::native
