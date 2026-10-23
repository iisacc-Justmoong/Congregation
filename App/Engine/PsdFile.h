#pragma once
#include <PsdPch.h>
#include <PsdFile.h>
#include <iiFileProviderCoreIO.h>
#include <limits>
namespace congregation
{
class PsdFile final : public psd::File
{
  public:
    PsdFile(psd::Allocator* allocator, const std::filesystem::path& path, bool write = false)
        : psd::File(allocator), write_(write)
    {
        open(path);
    }
    ~PsdFile() override { close(); }
    [[nodiscard]] bool isOpen() const { return channel_.has_value(); }
    [[nodiscard]] bool hasWriteError() const { return error_; }
    [[nodiscard]] std::uint64_t size() const
    {
        if (!channel_)
            return 0;
        auto n = channel_->size();
        return n ? *n : 0;
    }

  private:
    struct Operation
    {
        bool ok = false;
    };
    void open(const std::filesystem::path& path)
    {
        auto c = iiFileProvider::core::open_file(
            path, {write_ ? ii::io::Access::read_write : ii::io::Access::read,
                   write_ ? ii::io::Creation::truncate : ii::io::Creation::existing});
        if (c)
            channel_ = std::move(*c);
    }
    void close()
    {
        if (channel_)
        {
            if (write_ && !channel_->flush())
                error_ = true;
            (void) channel_->close();
            channel_.reset();
        }
    }
    bool DoOpenRead(const wchar_t* path) override
    {
        if (!channel_ && !write_)
            open(std::filesystem::path(path));
        return isOpen() && !write_;
    }
    bool DoOpenWrite(const wchar_t* path) override
    {
        if (!channel_ && write_)
            open(std::filesystem::path(path));
        return isOpen() && write_;
    }
    bool DoClose() override
    {
        close();
        return !error_;
    }
    ReadOperation DoRead(void* bytes, std::uint32_t count, std::uint64_t offset) override
    {
        auto* result = new Operation;
        if (!channel_ || offset > INT64_MAX)
            return result;
        auto seek = channel_->seek(static_cast<std::int64_t>(offset));
        if (!seek)
            return result;
        auto remaining = std::span(static_cast<std::byte*>(bytes), count);
        while (!remaining.empty())
        {
            auto read = channel_->read(remaining);
            if (!read || read->bytes == 0)
                return result;
            remaining = remaining.subspan(read->bytes);
        }
        result->ok = true;
        return result;
    }
    bool DoWaitForRead(ReadOperation& operation) override
    {
        auto* result = static_cast<Operation*>(operation);
        bool okay = result && result->ok;
        delete result;
        operation = nullptr;
        return okay;
    }
    WriteOperation DoWrite(const void* bytes, std::uint32_t count, std::uint64_t offset) override
    {
        auto* result = new Operation;
        if (!channel_ || !write_ || offset > INT64_MAX)
        {
            error_ = true;
            return result;
        }
        auto seek = channel_->seek(static_cast<std::int64_t>(offset));
        if (!seek)
        {
            error_ = true;
            return result;
        }
        auto remaining = std::span(static_cast<const std::byte*>(bytes), count);
        while (!remaining.empty())
        {
            auto written = channel_->write(remaining);
            if (!written || !*written)
            {
                error_ = true;
                return result;
            }
            remaining = remaining.subspan(*written);
        }
        result->ok = true;
        return result;
    }
    bool DoWaitForWrite(WriteOperation& operation) override { return DoWaitForRead(operation); }
    std::uint64_t DoGetSize() const override { return size(); }
    std::optional<ii::io::Channel> channel_;
    bool write_ = false, error_ = false;
};
} // namespace congregation
