#include "Preferences.h"
#include "Data.h"
#include "NearbyDiscovery.h"
#include <iiFileProviderNative.h>
#include <iiFileProviderCompatibility.h>
#include <cstdlib>
#include <sstream>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif
namespace congregation
{
namespace fs = std::filesystem;
namespace
{
fs::path env(const char* key)
{
#ifdef _WIN32
    std::wstring name;
    while (*key)
        name += static_cast<wchar_t>(*key++);
    const auto* value = _wgetenv(name.c_str());
    return value ? fs::path(value) : fs::path{};
#else
    const auto* value = std::getenv(key);
    if (!value)
        return {};
    auto bytes = std::string_view(value);
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(bytes.data()), bytes.size()));
#endif
}

bool as_bool(std::string s, bool fallback)
{
    s = lower(trim(std::move(s)));
    if (s == "true" || s == "1")
        return true;
    if (s == "false" || s == "0")
        return false;
    return fallback;
}
} // namespace
fs::path application_data()
{
#ifdef _WIN32
    auto root = env("LOCALAPPDATA");
#elif defined(__APPLE__)
    auto root = env("HOME") / "Library/Application Support";
#else
    auto root = env("XDG_DATA_HOME");
    if (root.empty())
        root = env("HOME") / ".local/share";
#endif
    return root.empty() ? fs::path{} : root / "iisacc/Congregation";
}
fs::path application_cache()
{
#ifdef _WIN32
    auto root = env("LOCALAPPDATA");
#elif defined(__APPLE__)
    auto root = env("HOME") / "Library/Caches";
#else
    auto root = env("XDG_CACHE_HOME");
    if (root.empty())
        root = env("HOME") / ".cache";
#endif
    return root.empty() ? fs::temp_directory_path() / "Congregation" : root / "iisacc/Congregation";
}
Preferences::Preferences(fs::path settings, fs::path storage)
{
    const bool native = settings.empty();
    settings_ = native ? application_data() / "preferences.ini" : std::move(settings);
    storage_ = storage.empty() ? application_data() / "canvas" : std::move(storage);
    std::error_code ec;
    auto input_path = settings_;
#ifndef __APPLE__
#ifndef _WIN32
    if (native && !fs::exists(input_path, ec))
    {
        auto config = env("XDG_CONFIG_HOME");
        if (config.empty())
            config = env("HOME") / ".config";
        input_path = config / "iisacc/Congregation.conf";
    }
#endif
#endif
    if (fs::exists(input_path, ec))
    {
        try
        {
            std::istringstream input(iiFileProvider::native::File::read(input_path, 1024 * 1024));
            std::string line, group;
            while (std::getline(input, line))
            {
                line = trim(line);
                if (line.empty() || line[0] == ';' || line[0] == '#')
                    continue;
                if (line[0] == '[' && line.back() == ']')
                {
                    group = line.substr(1, line.size() - 2);
                    if (group == "%General")
                        group = "General";
                    else if (group == "General")
                        group.clear();
                    continue;
                }
                auto pos = line.find('=');
                if (pos != std::string::npos)
                    values_[(group.empty() ? "" : group + "/") + trim(line.substr(0, pos))] =
                        trim(line.substr(pos + 1));
            }
        }
        catch (const iiFileProvider::native::FileError&)
        {
        }
    }
#ifdef __APPLE__
    // Read the former QSettings preference domain once, before native INI takes ownership.
    if (native && !fs::exists(settings_, ec))
    {
        for (auto key :
             {"General/startWithRecentCanvas", "General/discoverNearbyCongregationUsers"})
        {
            auto name = CFStringCreateWithCString(nullptr, key, kCFStringEncodingUTF8);
            auto value = CFPreferencesCopyAppValue(name, CFSTR("com.iisacc.Congregation"));
            CFRelease(name);
            if (value)
            {
                if (CFGetTypeID(value) == CFBooleanGetTypeID())
                    values_[key] =
                        CFBooleanGetValue(static_cast<CFBooleanRef>(value)) ? "true" : "false";
                CFRelease(value);
            }
        }
    }
#endif
#ifdef _WIN32
    if (native && !fs::exists(settings_, ec))
    {
        for (const auto& entry :
             {std::pair{L"startWithRecentCanvas", "General/startWithRecentCanvas"},
              std::pair{L"discoverNearbyCongregationUsers",
                        "General/discoverNearbyCongregationUsers"}})
        {
            DWORD value = 0, size = sizeof(value), type = 0;
            auto code = RegGetValueW(HKEY_CURRENT_USER, L"Software\\iisacc\\Congregation\\General",
                                     entry.first, RRF_RT_REG_DWORD, &type, &value, &size);
            if (code == ERROR_SUCCESS)
                values_[entry.second] = value ? "true" : "false";
            else
            {
                wchar_t text[16]{};
                size = sizeof(text);
                code = RegGetValueW(HKEY_CURRENT_USER, L"Software\\iisacc\\Congregation\\General",
                                    entry.first, RRF_RT_REG_SZ, &type, text, &size);
                if (code == ERROR_SUCCESS)
                    values_[entry.second] =
                        (std::wstring_view(text) == L"true" || std::wstring_view(text) == L"1")
                            ? "true"
                            : "false";
            }
        }
    }
#endif
    start_recent = values_.contains("General/startWithRecentCanvas")
                       ? as_bool(values_.at("General/startWithRecentCanvas"), false)
                       : false;
    discover_nearby = values_.contains("General/discoverNearbyCongregationUsers")
                          ? as_bool(values_.at("General/discoverNearbyCongregationUsers"), true)
                          : true;
    if(auto value=values_.find("Profile/invitationsAllowed");value!=values_.end()) invitations_allowed=as_bool(value->second,false);
    const auto removed = values_.erase("General/recentCanvasUrl");
    if(auto name=values_.find("Profile/name");name!=values_.end())profile_name=congregation::profile_name(name->second);
    if (!storage_.empty())
    {
        iiFileProvider::native::File::createDirectories(storage_);
        fs::permissions(storage_, fs::perms::owner_all, fs::perm_options::replace, ec);
    }
    if (removed)
        save();
}
void Preferences::save()
{
    values_["Profile/invitationsAllowed"] = invitations_allowed ? "true" : "false";
    values_["General/startWithRecentCanvas"] = start_recent ? "true" : "false";
    values_["General/discoverNearbyCongregationUsers"] = discover_nearby ? "true" : "false";
    std::string output, last;
    for (const auto& [key, value] : values_)
    {
        auto pos = key.rfind('/');
        auto group = pos == std::string::npos ? std::string{} : key.substr(0, pos);
        if (group != last || output.empty())
        {
            if (!output.empty())
                output += '\n';
            output += '[' +
                      (group == "General" ? "%General"
                       : group.empty()    ? "General"
                                          : group) +
                      "]\n";
            last = group;
        }
        output += key.substr(pos == std::string::npos ? 0 : pos + 1) + '=' + value + '\n';
    }
    iiFileProvider::native::File::createDirectories(settings_.parent_path());
    iiFileProvider::native::File::write(settings_, output);
}
void Preferences::set_start_recent(bool value)
{
    auto old = start_recent;
    start_recent = value;
    try
    {
        save();
    }
    catch (...)
    {
        start_recent = old;
        throw;
    }
}
void Preferences::set_invitations_allowed(bool value)
{
    const bool old=invitations_allowed;
    if(old==value)return;
    invitations_allowed=value;
    try{save();}catch(...){invitations_allowed=old;throw;}
}
void Preferences::set_profile_name(std::string value)
{
    value=congregation::profile_name(std::move(value));
    if(value.empty())value="Guest";
    if(profile_name==value)return;
    auto old=profile_name;profile_name=std::move(value);values_["Profile/name"]=profile_name;
    try{save();}catch(...){profile_name=old;values_["Profile/name"]=old;throw;}
}
bool Preferences::record_recovery(const fs::path& path)
{
    std::error_code ec;
    auto root=fs::canonical(storage_,ec);if(ec)return false;
    auto absolute=fs::absolute(path,ec).lexically_normal();if(ec||absolute.parent_path()!=root||absolute.extension()!=".iisc"||!canonical_uuid(absolute.stem().string()))return false;
    auto status=fs::symlink_status(absolute,ec);if(ec||!fs::is_regular_file(status)||fs::is_symlink(status))return false;
    const auto name=absolute.filename().string();if(values_["General/nativeRecoveryFile"]==name)return true;
    auto old=values_["General/nativeRecoveryFile"];values_["General/nativeRecoveryFile"]=name;
    try{save();return true;}catch(...){values_["General/nativeRecoveryFile"]=old;throw;}
}
void Preferences::set_discover_nearby(bool value)
{
    auto old = discover_nearby;
    discover_nearby = value;
    try
    {
        save();
    }
    catch (...)
    {
        discover_nearby = old;
        throw;
    }
}
fs::path Preferences::storage_path() const
{
    return storage_.empty() ? fs::path{} : (storage_ / "recent-canvas.vrc").lexically_normal();
}
fs::path Preferences::existing_canvas() const
{
    std::error_code ec;
    auto p = storage_path();
    if(auto current=values_.find("General/nativeRecoveryFile");current!=values_.end()){
        const fs::path name(current->second);
        if(name==name.filename()&&name.extension()==".iisc"&&canonical_uuid(name.stem().string())){
            const auto candidate=storage_/name;const auto status=fs::symlink_status(candidate,ec);
            if(!ec&&fs::is_regular_file(status)&&!fs::is_symlink(status))return fs::canonical(candidate,ec);
        }
    }
    auto status = fs::symlink_status(p, ec);
    return !ec && fs::is_regular_file(status) && !fs::is_symlink(status) ? fs::canonical(p, ec)
                                                                         : fs::path{};
}
bool Preferences::record_canvas(const fs::path& path) const
{
    std::error_code ec;
    auto expected = fs::absolute(storage_path(), ec).lexically_normal();
    if (ec)
        return false;
    auto requested = fs::absolute(path, ec).lexically_normal();
    return !ec && expected == requested && !existing_canvas().empty();
}
bool Preferences::clear_canvas()
{
    try
    {
        auto p = existing_canvas();
        if(p.empty())p=storage_path();
        if (p.empty())
            return true;
        if (fs::exists(fs::path(p.string() + ".iisacc-lock")))
            return false;
        if (!fs::exists(p))
            return true;
        iiFileProvider::native::FileWriteLock lock(p);
        (void) iiFileProvider::native::File::remove(p);
        return true;
    }
    catch (const iiFileProvider::native::FileError&)
    {
        return false;
    }
}
} // namespace congregation
