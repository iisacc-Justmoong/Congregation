#pragma once
#include "EditorBackend.h"
#include <filesystem>
#include <map>
#include <set>
namespace congregation::native
{
struct LibraryEntry final
{
    std::string name, metadata, icon, key;
    std::filesystem::path path;
};
class AssetLibraryView final : public std::enable_shared_from_this<AssetLibraryView>
{
  public:
    AssetLibraryView(std::shared_ptr<congregation::ui::EditorBackend>, std::filesystem::path);
    ii::gui::Widget build(ii::gui::WidgetBuilder&, ii::gui::Widget);

  private:
    void reload(std::string root);
    void refresh();
    void select(std::size_t);
    void insert();
    ii::core::Result<void> results(ii::gui::WidgetBuilder&);
    std::shared_ptr<congregation::ui::EditorBackend> vm_;
    std::filesystem::path bundled_;
    std::vector<LibraryEntry> entries_;
    std::vector<ii::gui::Widget> entry_rows_;
    std::shared_ptr<ii::gui::BindingScope> bindings_ = std::make_shared<ii::gui::BindingScope>();
    ii::gui::Widget results_, count_, preview_, name_, metadata_, location_;
    lvrs::Card preview_card_;
    std::map<std::string, ii::gui::Widget> collection_counts_;
    std::set<std::string> recent_, saved_;
    std::string scope_ = "All";
    std::string filter_, category_ = "All assets";
    std::size_t selected_ = 0;
};
} // namespace congregation::native
