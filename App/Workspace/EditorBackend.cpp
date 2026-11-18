#include "EditorBackend.h"
#include <stdexcept>
namespace congregation::ui
{
EditorBackend::EditorBackend()
{
    tool_selection_subscription_ = selected_tool.observe([this, lifetime = std::weak_ptr(lifetime_)](const auto& tool, auto) {
        if (lifetime.expired()) return;
        if (parameters->select_brush(tool)) sync_parameter_fields();
        if (!lifetime.expired()) inspected_tool.set(tool);
    });
    set_presentation_lists(std::make_shared<PresentationList>(), std::make_shared<PresentationList>());
    set_color_model(color);
    sync_parameter_fields();
}
void EditorBackend::submit_agent()
{
    if (agent_busy.get()) return;
    auto prompt=agent_prompt.get();
    auto present=agent_present;
    std::weak_ptr<int> lifetime=lifetime_;
    if (present) present();
    if (lifetime.expired()) return;
    if (prompt.empty() || prompt.size()>16384 || prompt.find_first_not_of(" \t\r\n")==std::string::npos) {
        agent_status.set("Enter a message of at most 16 KiB."); return;
    }
    auto callback=agent_submit;
    if (!callback) { agent_status.set("The Society workspace agent is unavailable."); return; }
    agent_busy.set(true); if (lifetime.expired()) return;
    agent_response.set(""); if (lifetime.expired()) return;
    agent_status.set("Submitting to Society…"); if (lifetime.expired()) return;
    callback(std::move(prompt));
}
void EditorBackend::receive_agent_update(const congregation::WorkspaceAgentUpdate& update, std::string_view submitted)
{
    using Phase=congregation::WorkspaceAgentUpdate::Phase;
    std::weak_ptr<int> lifetime=lifetime_;
    if (update.phase==Phase::accepted && agent_prompt.get()==submitted) {
        agent_prompt.set(""); if (lifetime.expired()) return;
    }
    if (!update.response.empty()) {
        auto response=update.response;
        constexpr std::size_t limit=60000;
        if (response.size()>limit) {
            auto end=limit;
            while (end>0 && (static_cast<unsigned char>(response[end]) & 0xc0)==0x80) --end;
            response.resize(end);
            response+="\n\n[Response truncated in this view. Open Society for the full response.]";
        }
        agent_response.set(std::move(response)); if (lifetime.expired()) return;
    }
    agent_status.set(update.status); if (lifetime.expired()) return;
    if (update.phase==Phase::completed || update.phase==Phase::failed || update.phase==Phase::uncertain)
        agent_busy.set(false);
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
void EditorBackend::sync_parameter_field(native::ToolParameter id)
{
    using P = native::ToolParameter;
    switch (id) {
    case P::size: brush_size.set(parameters->value(id)); break;
    case P::opacity: opacity.set(parameters->value(id) / 100); break;
    case P::flow: flow.set(parameters->value(id) / 100); break;
    case P::hardness: hardness.set(parameters->value(id) / 100); break;
    case P::spacing: spacing.set(parameters->value(id) / 100); break;
    case P::stabilization: stabilizer.set(parameters->value(id) / 100); break;
    case P::absolute_spacing: spacing_pixels.set(parameters->value(id)); break;
    case P::pressure_opacity_enabled: pressure_enabled.set(parameters->value(id) != 0); break;
    case P::pressure_curve_min: pressure_min.set(parameters->value(id)); break;
    case P::pressure_curve_mid: pressure_mid.set(parameters->value(id)); break;
    case P::pressure_curve_max: pressure_max.set(parameters->value(id)); break;
    default: break;
    }
}
void EditorBackend::sync_parameter_fields()
{
    using P = native::ToolParameter;
    for (const auto id : {P::size, P::opacity, P::flow, P::hardness, P::spacing,
                          P::stabilization, P::absolute_spacing, P::pressure_opacity_enabled,
                          P::pressure_curve_min, P::pressure_curve_mid, P::pressure_curve_max})
        sync_parameter_field(id);
}
void EditorBackend::set_parameter(native::ToolParameter id, double value)
{
    if (parameters->set(id, value)) {
        sync_parameter_field(id);
        if (id == native::ToolParameter::spacing) sync_parameter_field(native::ToolParameter::absolute_spacing);
        invoke(brush_changed);
    }
}
void EditorBackend::set_parameter_normalized(native::ToolParameter id, double value)
{
    if (parameters->set_normalized(id, value)) {
        sync_parameter_field(id);
        if (id == native::ToolParameter::spacing) sync_parameter_field(native::ToolParameter::absolute_spacing);
        invoke(brush_changed);
    }
}
void EditorBackend::step_parameter(native::ToolParameter id, int direction)
{
    if (parameters->step(id, direction)) {
        sync_parameter_field(id);
        if (id == native::ToolParameter::spacing) sync_parameter_field(native::ToolParameter::absolute_spacing);
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
    const auto inspected = name;
    const std::weak_ptr<int> lifetime = lifetime_;
    if (auto callback = tool_selected)
        callback(std::move(name));
    else
        selected_tool.set(std::move(name));
    if (!lifetime.expired()) inspected_tool.set(inspected);
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
