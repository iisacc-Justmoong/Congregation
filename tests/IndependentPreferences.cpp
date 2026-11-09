#include "IndependentPreferencesView.generated.h"
#include <iiGUI/Image.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class T> T take(ii::core::Result<T> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
ii::gui::Widget member(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view name) {
    const auto nodes=take(tree->snapshots());
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.accessibility_name==name;});
    if(found==nodes.end()) throw std::runtime_error("Preferences member did not mount: "+std::string(name));
    return take(tree->find(found->id));
}
ii::gui::Widget widget(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    const auto nodes=take(tree->snapshots());
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end()) throw std::runtime_error("Preferences control did not mount: "+std::string(suffix));
    return take(tree->find(found->id));
}
}

int main() {
    using View=lvui_generated::IndependentPreferencesView;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{480,360}));
    std::shared_ptr<View::Scope> scope;
    std::string selected;
    int avatar_requests=0;
    auto photo=take(ii::gui::rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    std::vector<congregation::ui::Action> tab_actions;
    good(host.set_content([&](auto& builder) {
        return View::build(builder,{},[&](auto& view) {
            auto& backend=view.backend_0;
            backend.profile_avatar.set(photo);
            backend.general_visible.set(false);
            backend.members_visible.set(true);
            backend.perform=[&backend,&tab_actions,&avatar_requests](congregation::ui::Action action) {
                if(action==congregation::ui::Action::open_avatar){++avatar_requests;return;}
                tab_actions.push_back(action);
                backend.general_visible.set(action==congregation::ui::Action::general_tab);
                backend.profile_visible.set(action==congregation::ui::Action::profile_tab);
                backend.members_visible.set(action==congregation::ui::Action::members_tab);
            };
            backend.member_selected=[&](std::string key){selected=std::move(key);};
            auto published=backend.member_source->replace({{.key="one",.label="First artist"},
                                                          {.key="two",.label="Second artist"}});
            if(!published)throw std::runtime_error(published.error().message);
        },&scope);
    }));
    const auto initial_frame=take(tree->drawing_snapshot({480,360}));
    auto members_tab=widget(tree,"/members-tab"), general_tab=widget(tree,"/general-tab"),profile_tab=widget(tree,"/profile-tab");
    require(take(members_tab.snapshot()).checked && !take(general_tab.snapshot()).checked,
            "Preferences did not show the selected tab");
    require(take(members_tab.snapshot()).bounds.height==22&&take(general_tab.snapshot()).bounds.height==22&&
            take(profile_tab.snapshot()).bounds.height==22,
            "Preferences segments did not use the standard 22px LVUI button height");
    require(take(widget(tree,"/preferences-sections").snapshot()).bounds.height==29,
            "Preferences did not use the standard LVUI segmented-control geometry");
    const auto has_title=[](const auto& frame,std::string_view label){
        const auto contains=[&](const auto& texts){return std::ranges::any_of(texts,[&](const auto& text){
            return text.text==label&&std::abs(text.color.red-1)<.0001&&std::abs(text.color.green-1)<.0001&&
                   std::abs(text.color.blue-1)<.0001&&std::abs(text.color.alpha-.9)<.0001;});};
        return contains(frame.texts)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return contains(layer.texts);});
    };
    require(has_title(initial_frame,"Members"),"Selected Preferences segment did not use the default LVUI title color");
    good(members_tab.activate());
    require(take(members_tab.snapshot()).checked, "Clicking the selected tab cleared its selection");
    good(general_tab.activate());
    require(take(general_tab.snapshot()).checked && !take(members_tab.snapshot()).checked,
            "Preferences tabs did not follow the shared selection state");
    good(profile_tab.activate());require(take(profile_tab.snapshot()).checked && !take(general_tab.snapshot()).checked,
            "Profile tab did not follow the shared selection state");
    const auto profile_frame=take(tree->drawing_snapshot({480,360}));
    require(has_title(profile_frame,"Profile"),"Selected Preferences segment did not update its LVUI title color");
    const auto has_description=[](const auto& texts){return std::ranges::any_of(texts,[](const auto& text){
        return text.text=="User image"&&text.font_size==12&&text.line_height==12&&text.font_weight==600&&
               std::abs(text.color.alpha-.6)<.0001;
    });};
    require(has_description(profile_frame.texts)||std::ranges::any_of(profile_frame.overlays,[&](const auto& layer){
        return has_description(layer.texts);}),"Profile label lost the common LVUI description style");
    auto profile_image=widget(tree,"/profile-image"),avatar=widget(tree,"/profile-avatar"),name=widget(tree,"/profile-name");
    require(take(avatar.snapshot()).visible&&take(avatar.snapshot()).size.width==64&&
            take(avatar.snapshot()).size.height==64,"Profile did not use the LVUI large Avatar");
    const auto contains_photo=[&](const ii::gui::CanvasFrame& frame){
        if(std::ranges::any_of(frame.images,[&](const auto& run){return run.image==photo;}))return true;
        return std::ranges::any_of(frame.overlays,[&](const auto& layer){return std::ranges::any_of(
            layer.images,[&](const auto& run){return run.image==photo;});});
    };
    require(contains_photo(take(tree->drawing_snapshot({480,360}))),"Profile Avatar did not show the selected photo");
    scope->backend_0.profile_avatar.set({});
    const auto cleared=take(tree->drawing_snapshot({480,360}));
    const auto placeholder=take(lvui::native::avatar_placeholder_mask_image({64,64},1));
    const auto is_placeholder=[&](const auto& run){return run.image==placeholder&&run.tint&&
        std::abs(run.tint->red-1)<.0001&&std::abs(run.tint->green-1)<.0001&&
        std::abs(run.tint->blue-1)<.0001&&std::abs(run.tint->alpha-.8)<.0001;};
    require(!contains_photo(cleared)&&
        (std::ranges::any_of(cleared.images,is_placeholder)||
         std::ranges::any_of(cleared.overlays,[&](const auto& layer){return std::ranges::any_of(
             layer.images,is_placeholder);})),
        "Profile Avatar did not restore the LVUI placeholder after clearing the photo");
    scope->backend_0.profile_avatar.set(photo);
    require(contains_photo(take(tree->drawing_snapshot({480,360}))),"Profile Avatar did not update after selecting a photo");
    require(take(name.snapshot()).size.width==432&&take(name.snapshot()).size.height==22,
            "Profile name did not use the Figma content width and compact LVUI input");
    const auto field_frame=take(tree->drawing_snapshot({480,360}));
    require(std::ranges::any_of(field_frame.overlays,[](const auto& layer){return layer.backdrop&&
        layer.backdrop->blur_radius==8&&layer.backdrop->bounds.height==22;}),
        "Profile name did not use the rounded LVUI input material");
    auto clear=widget(tree,"/profile-name.clear");
    require(take(clear.snapshot()).visible,"Profile name did not expose its LVUI clear action");
    const auto image_bounds=take(profile_image.snapshot()).bounds;
    require(image_bounds.width==64&&image_bounds.height==64,
            "Profile Avatar input region did not follow the common Avatar's natural size");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=image_bounds.x+100,
        .y=image_bounds.y+image_bounds.height/2,.timestamp=.5,.buttons=1}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=image_bounds.x+100,
        .y=image_bounds.y+image_bounds.height/2,.timestamp=.6}));
    require(avatar_requests==0,"Profile Avatar menu opened outside its visible bounds");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=image_bounds.x+image_bounds.width/2,
        .y=image_bounds.y+image_bounds.height/2,.timestamp=1,.buttons=1}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=image_bounds.x+image_bounds.width/2,
        .y=image_bounds.y+image_bounds.height/2,.timestamp=1.1}));
    require(avatar_requests==1,"The profile Avatar did not open its menu on pointer release");
    good(clear.activate());
    require(scope->backend_0.display_name.get().empty(),"The profile input clear action did not update the view model");
    good(members_tab.activate());
    require(tab_actions==std::vector<congregation::ui::Action>{congregation::ui::Action::members_tab,
        congregation::ui::Action::general_tab,congregation::ui::Action::profile_tab,congregation::ui::Action::members_tab},
        "Preferences tab activation did not issue exactly one action per click");
    good(member(tree,"First artist").activate());
    require(selected=="one" && scope->backend_0.member_revision.get()==1,
            "Member activation lost its key or changed the shared model");
    auto published=scope->backend_0.member_source->replace({{.key="two",.label="Updated artist"}});
    require(published && *published && scope->backend_0.member_revision.get()==2,
            "Member model did not publish its revision");
    (void)take(tree->drawing_snapshot({480,360}));
    good(member(tree,"Updated artist").activate());
    require(selected=="two" && scope->backend_0.member_revision.get()==2,
            "Updated member activation lost its key or mutated the model");
    auto join=widget(tree,"/join-session"),popup=widget(tree,"/join-popup");
    good(popup.open_for(join,0,take(join.snapshot()).bounds.height));
    (void)take(tree->drawing_snapshot({480,360}));
    require(take(popup.snapshot()).bounds.width==344&&take(widget(tree,"/session-address").snapshot()).bounds.width==320,
            "Join popover did not retain its input width and padding");
    auto graph=scope->native_lifetime.lock();
    require(graph && take(graph->failures()).empty(),"Preferences has an unresolved reactive binding failure");
    std::weak_ptr<View::Scope> weak=scope;
    good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired(),"Preferences retained its retired scope");
    tree->close();
}
