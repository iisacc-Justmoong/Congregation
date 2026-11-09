# iPadOS LVUI host bindings

Congregation composes current LVUI control policies with iiGUI native hosts in
`App/Native/LVUIHostBindings.h`. The same bindings are used by the native
application on supported platforms; no platform-specific copy of the LVUI
control implementation is required. The LVUI compiler, installed headers and
libraries must come from the same SDK generation. The iPad SDK is staged under
`SDK/build/congregation-ipados/prefix` by `tools/build_ipados_sdk.py`.

`tests/test_lvui_ipados_bindings.py` detects direct references to retired
LVUI native host types. Build the application with the iPad SDK and run the
native control tests separately: a source boundary check is not evidence of
working device rendering or input.

The color editor compiles LVUI's installed `examples/Integrations/App/ColorSelectionSurface.cpp`
as part of `CongregationEditorColor` and links iiColorManager. `LVUI::Color`
is an interface contract and does not itself provide the application adapter.

On 2026-11-08, the iPhoneOS arm64 Release application built and was signed with
an Apple Development identity, then installed over the existing app on a paired
iPad Pro (12.9-inch, 4th generation). The reported bundle version was 1.0.2
(3). The installation was verified with `devicectl`; launch, touch input and
rendering were not part of that verification.
