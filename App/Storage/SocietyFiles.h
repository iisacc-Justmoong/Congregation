#pragma once
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace congregation::native {
// A validated, pinned Society Files section. Never creates a private fallback.
class SocietyFiles final {
public:
    static std::expected<SocietyFiles,std::string> discover();
    static std::expected<SocietyFiles,std::string> from_registry(const std::filesystem::path& registry);
    static std::expected<SocietyFiles,std::string> from_group(const std::filesystem::path& group);
    // A system group is used only when it contains a valid Society selection.
    // Otherwise mobile applications have separate sandbox-owned packages.
    static std::expected<SocietyFiles,std::string> from_mobile(
        const std::filesystem::path& application_storage,
        const std::optional<std::filesystem::path>& system_group = {}, bool migrate = false);
    [[nodiscard]] const std::filesystem::path& root() const noexcept { return files_; }
    [[nodiscard]] std::expected<std::filesystem::path,std::string>
        destination(const std::filesystem::path& path) const;
    [[nodiscard]] bool available() const;
private:
    SocietyFiles(std::filesystem::path container,std::filesystem::path files,std::string id,
                 std::filesystem::path registry = {})
        : container_(std::move(container)),files_(std::move(files)),registry_(std::move(registry)),id_(std::move(id)) {}
    std::filesystem::path container_,files_,registry_,group_;
    std::string id_,storage_id_,storage_kind_,owner_;
};
}
