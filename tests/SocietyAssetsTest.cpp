#include <Native/SocietyAssets.h>
#include <chrono>
#include <fstream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2) throw std::runtime_error("fixture directory required");
    namespace fs = std::filesystem;
    const fs::path fixture = fs::path(argv[1]) / ("society-assets-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{fixture};
    const auto container = fixture / "container";
    fs::create_directories(container / "Asset Library/nested");
    const auto put = [](const fs::path& path, const std::string& text) { std::ofstream f(path); f << text; if (!f) throw std::runtime_error("Cannot write fixture"); };
    put(container / ".society-drive.json", R"({"identifier":"fixture","sections":[{"id":"asset-library","path":"Asset Library"}]})");
    put(container / "Asset Library/image.png", "fixture");
    put(container / "Asset Library/nested/brush.dat", "fixture");
    using ii::core::Value;
    const auto registry = [&](const char* id) {
        return ii::network::wire::json(Value(Value::Object{{"schemaVersion", Value(1)}, {"containerId", Value(id)}, {"path", Value(container.string())}}));
    };
    auto valid = registry("fixture"), invalid = registry("wrong");
    if (valid.empty() || invalid.empty()) throw std::runtime_error("Cannot create registry fixture");
    put(fixture / "storage.json", valid);
    put(fixture / "invalid.json", invalid);
    put(fixture / "malformed.json", "{invalid JSON");
    const auto parsed = congregation::native::read_society_json(fixture / "storage.json");
    if (!parsed || ii::network::wire::text(ii::network::wire::field(*parsed, "containerId")) != "fixture")
        throw std::runtime_error("Society registry codec did not preserve the Core value");
    if (congregation::native::read_society_json(fixture / "malformed.json"))
        throw std::runtime_error("Malformed Society registry was accepted");
    auto loaded = congregation::native::society_assets(fixture / "storage.json");
    if (!loaded.error.empty() || loaded.files.size() != 2 || loaded.files[0].filename() != "image.png" || loaded.files[1].filename() != "brush.dat")
        throw std::runtime_error("Society asset listing failed: " + loaded.error +
                                 "; file count=" + std::to_string(loaded.files.size()));
    if (congregation::native::society_assets(fixture / "invalid.json").error.empty())
        throw std::runtime_error("Mismatched Society identity accepted");
    if (congregation::native::society_assets(fixture / "missing.json").error.empty())
        throw std::runtime_error("Missing Society registry accepted");
}
