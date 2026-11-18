#pragma once
#include <filesystem>
#include <map>
#include <string>
namespace congregation
{
std::filesystem::path application_data();
std::filesystem::path application_cache();
class Preferences final
{
  public:
    Preferences(std::filesystem::path settings = {}, std::filesystem::path storage = {});
    bool start_recent = false, discover_nearby = true, invitations_allowed = false;
    std::string profile_name="Guest";
    void set_start_recent(bool);
    void set_discover_nearby(bool);
    void set_profile_name(std::string);
    void set_invitations_allowed(bool);
    bool record_recovery(const std::filesystem::path&);
    [[nodiscard]] std::filesystem::path storage_path() const;
    [[nodiscard]] std::filesystem::path existing_canvas() const;
    bool record_canvas(const std::filesystem::path&) const;
    bool clear_canvas();

  private:
    void save();
    std::filesystem::path settings_, storage_;
    std::map<std::string, std::string> values_;
};
} // namespace congregation
