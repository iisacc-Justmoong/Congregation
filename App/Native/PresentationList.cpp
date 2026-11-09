#include "PresentationList.h"

#include <utility>

namespace congregation::ui
{
namespace
{
constexpr std::uint64_t maximum_revision = 9007199254740991ULL;
}

PresentationList::PresentationList()
    : rows_(std::make_shared<const std::vector<PresentationRow>>()),
      projection_(std::make_shared<const lvui::ListSnapshot>())
{
}

lvui::ListResult<std::shared_ptr<const lvui::ListSnapshot>> PresentationList::snapshot() const
{
    std::scoped_lock lock(mutex_);
    return projection_;
}

std::shared_ptr<const std::vector<PresentationRow>> PresentationList::rows() const
{
    std::scoped_lock lock(mutex_);
    return rows_;
}

lvui::ListResult<bool> PresentationList::replace(std::vector<PresentationRow> rows)
{
    if (std::this_thread::get_id() != owner_)
        return std::unexpected(lvui::ListError{lvui::ListErrorCode::wrong_thread,
                                               "Presentation list requires its owner thread"});
    auto projected = std::make_shared<lvui::ListSnapshot>();
    projected->entries.reserve(rows.size());
    for (const auto& row : rows)
    {
        if (row.parent.size() > 1024 || !lvui::valid_accessibility_text(row.parent))
            return std::unexpected(lvui::ListError{lvui::ListErrorCode::invalid,
                                                   "Invalid presentation parent key"});
        projected->entries.push_back({.key = row.key, .label = row.label,
                                      .enabled = row.enabled, .selectable = row.enabled,
                                      .editable = row.editable, .draggable = row.draggable});
    }
    if (auto valid = lvui::ListIndex::create(projected); !valid)
        return std::unexpected(valid.error());
    std::uint64_t next;
    {
        std::scoped_lock lock(mutex_);
        if (*rows_ == rows) return false;
        if (projection_->revision >= maximum_revision)
            return std::unexpected(lvui::ListError{lvui::ListErrorCode::exhausted,
                                                   "Presentation list revision exhausted"});
        next = projection_->revision + 1;
        projected->revision = next;
        rows_ = std::make_shared<const std::vector<PresentationRow>>(std::move(rows));
        projection_ = std::move(projected);
    }
    revision.set(static_cast<std::int64_t>(next));
    return true;
}
} // namespace congregation::ui
