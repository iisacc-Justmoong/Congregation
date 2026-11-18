#pragma once

#include <iiSDKCore>
#include <vector>

namespace congregation::ui { class EditorBackend; }
namespace congregation::native {
class CanvasView;

// Owns the subscriptions connecting the current editor to its canvas. Both
// endpoints remain owned by the application; destroying this object detaches.
class CanvasToolBindings final {
public:
    CanvasToolBindings(ui::EditorBackend&, CanvasView&);
    CanvasToolBindings(const CanvasToolBindings&) = delete;
    CanvasToolBindings& operator=(const CanvasToolBindings&) = delete;

private:
    std::vector<ii::core::ObservableSubscription> subscriptions_;
};
}
