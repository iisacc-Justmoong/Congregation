#include "PresentationList.h"

#include <algorithm>
#include <utility>
#include <unordered_map>

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
lvui::ListResult<bool> PresentationList::replace_row(PresentationRow row)
{
    if (std::this_thread::get_id() != owner_)
        return std::unexpected(lvui::ListError{lvui::ListErrorCode::wrong_thread,
                                               "Presentation list requires its owner thread"});
    if (row.parent.size() > 1024 || !lvui::valid_accessibility_text(row.parent))
        return std::unexpected(lvui::ListError{lvui::ListErrorCode::invalid, "Invalid presentation parent key"});
    std::shared_ptr<const std::vector<PresentationRow>> prior_rows;
    std::shared_ptr<const lvui::ListSnapshot> prior_projection;
    {
        std::scoped_lock lock(mutex_);
        prior_rows = rows_; prior_projection = projection_;
    }
    const auto found = std::ranges::find(*prior_rows, row.key, &PresentationRow::key);
    if (found == prior_rows->end())
        return std::unexpected(lvui::ListError{lvui::ListErrorCode::not_found, "Presentation row not found"});
    if (*found == row) return false;
    if (prior_projection->revision >= maximum_revision)
        return std::unexpected(lvui::ListError{lvui::ListErrorCode::exhausted, "Presentation list revision exhausted"});
    const auto index = std::size_t(found - prior_rows->begin());
    auto next_rows = std::make_shared<std::vector<PresentationRow>>(*prior_rows);
    auto next_projection = std::make_shared<lvui::ListSnapshot>(*prior_projection);
    (*next_rows)[index] = std::move(row);
    const auto &value = (*next_rows)[index];
    next_projection->entries[index] = {.key = value.key, .label = value.label,
        .enabled = value.enabled, .selectable = value.enabled,
        .editable = value.editable, .draggable = value.draggable};
    ++next_projection->revision;
    if (auto valid = lvui::ListIndex::create(next_projection); !valid) return std::unexpected(valid.error());
    const auto next = next_projection->revision;
    {
        std::scoped_lock lock(mutex_);
        rows_ = std::move(next_rows); projection_ = std::move(next_projection);
    }
    revision.set(static_cast<std::int64_t>(next));
    return true;
}
lvui::ListResult<bool> PresentationList::apply_rows(
    std::span<const std::string> order, std::span<const PresentationRow> edits)
{
    if (std::this_thread::get_id() != owner_)
        return std::unexpected(lvui::ListError{lvui::ListErrorCode::wrong_thread,
                                               "Presentation list requires its owner thread"});
    const auto prior = rows();
    const auto priorProjection = snapshot().value();
    std::unordered_map<std::string_view, std::size_t> index;
    index.reserve(prior->size());
    for (std::size_t i = 0; i < prior->size(); ++i) index.emplace((*prior)[i].key, i);
    std::unordered_map<std::string_view, const PresentationRow*> replacements;
    for (const auto& row : edits) replacements.emplace(row.key, &row);
    auto rows = std::make_shared<std::vector<PresentationRow>>();
    auto projection = std::make_shared<lvui::ListSnapshot>();
    rows->reserve(order.size());
    projection->entries.reserve(order.size());
    for (const auto& key : order) {
        if (auto edit = replacements.find(key); edit != replacements.end()) {
            const auto& row = *edit->second;
            if (row.parent.size() > 1024 || !lvui::valid_accessibility_text(row.parent))
                return std::unexpected(lvui::ListError{lvui::ListErrorCode::invalid, "Invalid presentation parent key"});
            rows->push_back(row);
            projection->entries.push_back({.key = row.key, .label = row.label,
                .enabled = row.enabled, .selectable = row.enabled, .editable = row.editable, .draggable = row.draggable});
        } else if (auto retained = index.find(key); retained != index.end()) {
            rows->push_back((*prior)[retained->second]);
            projection->entries.push_back(priorProjection->entries[retained->second]);
        } else return std::unexpected(lvui::ListError{lvui::ListErrorCode::not_found, "Presentation row not found"});
    }
    if (*rows == *prior) return false;
    if (priorProjection->revision >= maximum_revision)
        return std::unexpected(lvui::ListError{lvui::ListErrorCode::exhausted, "Presentation list revision exhausted"});
    projection->revision = priorProjection->revision + 1;
    if (auto valid = lvui::ListIndex::create(projection); !valid) return std::unexpected(valid.error());
    {
        std::scoped_lock lock(mutex_);
        rows_ = std::move(rows);
        projection_ = projection;
    }
    revision.set(static_cast<std::int64_t>(projection->revision));
    return true;
}
} // namespace congregation::ui
