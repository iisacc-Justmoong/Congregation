#include <Native/AppMenu.h>
#include <array>
#include <stdexcept>
#include <string_view>

int main()
{
    using congregation::ui::Action;
    Action invoked = Action::quit;
    const auto menu = congregation::native::application_menu([&](Action action) { invoked = action; });
    constexpr std::array<std::string_view, 9> expected{
        "File", "Edit", "Select", "View", "Layer", "Image", "Filter", "Window", "Help"};
    if (menu.size() != expected.size()) throw std::runtime_error("incorrect menu count");
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (menu[i].title != expected[i] || menu[i].children.empty())
            throw std::runtime_error("incorrect menu order or empty menu");
    menu[3].children[0].triggered();
    if (invoked != Action::fit) throw std::runtime_error("View command is disconnected");
    menu[4].children[0].triggered();
    if (invoked != Action::add_layer) throw std::runtime_error("Layer command is disconnected");
    if (menu[6].children[0].enabled || menu[8].children[0].enabled)
        throw std::runtime_error("Unavailable commands must be disabled");
}
