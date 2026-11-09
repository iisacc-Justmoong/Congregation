#include "EditorBackend.h"
#include <stdexcept>
namespace congregation::ui
{
EditorBackend::EditorBackend()
{
    set_presentation_lists(std::make_shared<PresentationList>(), std::make_shared<PresentationList>());
    set_color_model(color);
    sync_parameter_fields();
}
void EditorBackend::set_presentation_lists(std::shared_ptr<PresentationList> layers,
                                           std::shared_ptr<PresentationList> members)
{
    if (!layers || !members) throw std::invalid_argument("Presentation lists are required");
    auto generation = std::make_shared<int>(0);
    std::weak_ptr<int> lifetime = lifetime_;
    auto layer_lease = layers->revision.observe(
        [this, lifetime, generation](const std::int64_t& value, std::uint64_t) {
            if (!lifetime.expired() && lists_generation_ == generation) layer_revision.set(value);
        });
    auto member_lease = members->revision.observe(
        [this, lifetime, generation](const std::int64_t& value, std::uint64_t) {
            if (!lifetime.expired() && lists_generation_ == generation) member_revision.set(value);
        });
    const auto layer_value = layers->revision.get();
    const auto member_value = members->revision.get();
    layer_source_subscription_ = std::move(layer_lease);
    member_source_subscription_ = std::move(member_lease);
    layer_source = std::move(layers);
    member_source = std::move(members);
    lists_generation_ = generation;
    layer_revision.set(layer_value);
    if (lifetime.expired() || lists_generation_ != generation) return;
    member_revision.set(member_value);
}
void EditorBackend::set_color_model(std::shared_ptr<EditorColor> model)
{
    if (!model) throw std::invalid_argument("Color model is required");
    std::weak_ptr<EditorColor> weak = model;
    std::weak_ptr<int> lifetime = lifetime_;
    auto generation = std::make_shared<int>(0);
    auto observer = model->revision.observe(
        [this, weak, lifetime, generation](const std::int64_t& revision, std::uint64_t) {
            if (lifetime.expired() || color_generation_ != generation) return;
            if (const auto source = weak.lock()) {
                current_color.set(source->current());
                if (lifetime.expired() || color_generation_ != generation ||
                    source->revision.get() != revision) return;
                color_revision.set(revision);
            }
        });
    auto source = model->source();
    color = model;
    color_subscription_ = std::move(observer);
    color_generation_ = generation;
    color_source.set(std::move(source));
    if (lifetime.expired() || color_generation_ != generation) return;
    const auto revision = model->revision.get();
    current_color.set(model->current());
    if (lifetime.expired() || color_generation_ != generation || model->revision.get() != revision) return;
    color_revision.set(revision);
}
void EditorBackend::sync_parameter_fields()
{
    using P = native::ToolParameter;
    brush_size.set(parameters->value(P::size));
    opacity.set(parameters->value(P::opacity) / 100);
    flow.set(parameters->value(P::flow) / 100);
    hardness.set(parameters->value(P::hardness) / 100);
    spacing.set(parameters->value(P::spacing) / 100);
    stabilizer.set(parameters->value(P::stabilization) / 100);
}
void EditorBackend::set_parameter(native::ToolParameter id, double value)
{
    if (parameters->set(id, value))
    {
        sync_parameter_fields();
        invoke(brush_changed);
    }
}
void EditorBackend::set_parameter_normalized(native::ToolParameter id, double value)
{
    if (parameters->set_normalized(id, value))
    {
        sync_parameter_fields();
        invoke(brush_changed);
    }
}
void EditorBackend::step_parameter(native::ToolParameter id, int direction)
{
    if (parameters->step(id, direction))
    {
        sync_parameter_fields();
        invoke(brush_changed);
    }
}
void EditorBackend::reset_parameters()
{
    parameters->reset();
    sync_parameter_fields();
    invoke(brush_changed);
}
void EditorBackend::select_tool(std::string name)
{
    if (auto callback = tool_selected)
        callback(std::move(name));
    else
        selected_tool.set(std::move(name));
}
void EditorBackend::choose_preset()
{
    if (preset.get().starts_with("Textured"))
    {
        parameters->reset();
        parameters->set(native::ToolParameter::size, 24);
        parameters->set(native::ToolParameter::hardness, 100);
        parameters->set(native::ToolParameter::wetness, 0);
        preset.set("Graphite pencil · Normal");
    }
    else
    {
        parameters->reset();
        preset.set("Textured oil · Normal");
    }
    sync_parameter_fields();
    invoke(brush_changed);
}
}
