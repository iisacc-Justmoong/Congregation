#include <Native/PresentationList.h>

#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        using congregation::ui::PresentationList;
        using congregation::ui::PresentationRow;
        auto model = std::make_shared<PresentationList>();
        auto left = lvui::ListController::create(model).value();
        auto right = lvui::ListController::create(model).value();
        unsigned notifications = 0;
        auto subscription = model->revision.observe([&](const auto&, auto) { ++notifications; });
        const std::vector<PresentationRow> initial{
            {.key = "layer-a", .label = "Layer A", .editable = true, .checked = true, .draggable = true},
            {.key = "layer-b", .label = "Layer B", .editable = true, .checked = false},
            {.key = "self", .label = "Me", .enabled = false},
            {.key = "member", .label = "Artist", .editable = false}};

        require(model->replace(initial).value(), "initial rows were not published");
        require(model->revision.get() == 1 && notifications == 1, "revision was not published once");
        auto rows = model->rows();
        require(rows->size() == 4 && !rows->at(1).checked, "row metadata was lost");
        const auto source = model->snapshot().value();
        require(source->entries[0].key == "layer-a" && source->entries[1].label == "Layer B" &&
                    !source->entries[2].enabled && source->entries[0].editable && !source->entries[3].editable &&
                    source->entries[0].draggable && !source->entries[3].draggable && source->revision == 1,
                "LVUI list projection disagrees with the source rows");
        require(!model->replace(initial).value() && notifications == 1,
                "identical rows produced a spurious update");
        require(left.refresh().value() && right.refresh().value(), "controllers failed to refresh");
        require(left.select("layer-a").value(), "left selection failed");
        require(right.select("layer-b").value(), "right selection failed");
        require(left.selection().value() == "layer-a" && right.selection().value() == "layer-b" &&
                    model->revision.get() == 1,
                "view selection altered another view or the source revision");
        auto invalid = initial;
        invalid[1].key = invalid[0].key;
        require(!model->replace(invalid) && model->revision.get() == 1 &&
                    model->snapshot().value() == source && model->rows() == rows,
                "invalid candidate replaced the last valid snapshot");
        auto updated = initial;
        updated.erase(updated.begin());
        updated[0].checked = true;
        require(model->replace(updated).value() && model->revision.get() == 2,
                "updated rows were not published");
        require(left.refresh().value() && right.refresh().value(), "changed list did not refresh");
        require(left.selection().value().empty() && right.selection().value() == "layer-b",
                "refresh did not preserve or clear selections by key");
        require(rows->size() == 4 && !rows->at(1).checked,
                "a previous snapshot changed after publication");
        bool foreign_rejected = false;
        std::thread worker([&] { foreign_rejected = !model->replace(initial); });
        worker.join();
        require(foreign_rejected && model->revision.get() == 2,
                "foreign-thread mutation changed the presentation model");
        std::cout << "PRESENTATION_LIST_OK\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
