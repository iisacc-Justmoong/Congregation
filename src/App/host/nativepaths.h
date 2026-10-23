#pragma once
#include <Engine/Data.h>
#include <Engine/Preferences.h>
#include <iiFileProviderCompatibility.h>
#include <iiFileProviderNative.h>
#include <filesystem>
#include <QString>
#include <QByteArray>
namespace congregation::presentation
{
inline std::filesystem::path nativePath(const QString& s)
{
    auto utf8 = s.toUtf8();
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(utf8.constData()), utf8.size()));
}
inline QString pathText(const std::filesystem::path& p)
{
    auto s = p.u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(s.data()), s.size());
}
inline bool ownerOnly(const QString& p, bool directory = false)
{
    std::error_code ec;
    std::filesystem::permissions(nativePath(p),
                                 directory ? std::filesystem::perms::owner_all
                                           : std::filesystem::perms::owner_read |
                                                 std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, ec);
    return !ec;
}
inline bool exists(const QString& p)
{
    std::error_code ec;
    return std::filesystem::exists(nativePath(p), ec);
}
inline QByteArray readFile(const QString& p, std::uint64_t maximum = UINT64_MAX)
{
    return QByteArray::fromStdString(iiFileProvider::native::File::read(nativePath(p), maximum));
}
inline void writeFile(const QString& p, const QByteArray& bytes)
{
    iiFileProvider::native::writeCompatible(nativePath(p),
                                            std::string_view(bytes.constData(), bytes.size()));
}
inline bool removeFile(const QString& p)
{
    return iiFileProvider::native::File::remove(nativePath(p));
}
inline bool regularFile(const QString& p)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(nativePath(p), ec);
}
inline bool symbolicLink(const QString& p)
{
    std::error_code ec;
    return std::filesystem::is_symlink(nativePath(p), ec);
}
inline std::uint64_t fileSize(const QString& p)
{
    std::error_code ec;
    auto n = std::filesystem::file_size(nativePath(p), ec);
    return ec ? 0 : n;
}
inline std::int64_t modifiedMilliseconds(const QString& p)
{
    std::error_code ec;
    auto time = std::filesystem::last_write_time(nativePath(p), ec);
    return ec ? 0
              : std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::filesystem::file_time_type::clock::to_sys(time).time_since_epoch())
                    .count();
}
} // namespace congregation::presentation
