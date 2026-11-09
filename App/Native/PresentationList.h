#pragma once

#include <LVUI/List.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace congregation::ui
{
// These are presentation values, not document edits. Product code owns their
// meaning while LVUI reads the stable key and label through ListSource.
struct PresentationRow final
{
    std::string key, label, parent;
    bool enabled = true, editable = false, checked = true, draggable = false;
    friend bool operator==(const PresentationRow&, const PresentationRow&) = default;
};

class PresentationList final : public lvui::ListSource
{
  public:
    PresentationList();

    [[nodiscard]] lvui::ListResult<std::shared_ptr<const lvui::ListSnapshot>> snapshot() const override;
    [[nodiscard]] std::shared_ptr<const std::vector<PresentationRow>> rows() const;
    [[nodiscard]] lvui::ListResult<bool> replace(std::vector<PresentationRow>);

    // Publish this Core value to LVUI's modelRevision input. Selection belongs
    // to each LVUI ListController and does not increment this source revision.
    ii::core::Observable<std::int64_t> revision{0};

  private:
    const std::thread::id owner_ = std::this_thread::get_id();
    mutable std::mutex mutex_;
    std::shared_ptr<const std::vector<PresentationRow>> rows_;
    std::shared_ptr<const lvui::ListSnapshot> projection_;
};
} // namespace congregation::ui
