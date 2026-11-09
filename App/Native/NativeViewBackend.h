#pragma once

#include <iiGUI/NativeView.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <exception>
#include <memory>
#include <utility>

namespace congregation::native
{
// The LVUI scope owns this source; each NativeView activation owns its own
// mounted widget generation. Sources must not retain their owning scope.
struct NativeViewBackend final
{
    ii::core::Observable<std::shared_ptr<ii::gui::NativeViewSource>> source{nullptr};
};

class NativeSurfaceSource final : public ii::gui::NativeViewSource
{
  public:
    explicit NativeSurfaceSource(ii::gui::WidgetFactory factory) : factory_(std::move(factory)) {}

    ii::core::Result<void> build(ii::gui::WidgetBuilder &builder) const override
    {
        if (!factory_)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Native surface has no factory");
        try { return factory_(builder); }
        catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Native surface construction failed",
                                  std::current_exception());
        }
    }

  private:
    ii::gui::WidgetFactory factory_;
};
} // namespace congregation::native
