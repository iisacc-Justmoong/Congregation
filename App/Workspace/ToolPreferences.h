#pragma once
#include <expected>
#include <filesystem>
#include <string>

namespace congregation::ui {
class EditorBackend;

class ToolPreferences final {
public:
    explicit ToolPreferences(std::filesystem::path directory);
    [[nodiscard]] std::expected<void, std::string> save(const EditorBackend&) const;
    // false means no saved preferences; an error means no values were applied.
    [[nodiscard]] std::expected<bool, std::string> load(EditorBackend&) const;
private:
    std::filesystem::path directory_;
};
}
