#pragma once
#include "pke/ApplicationFlow.hpp"
#include "pke/PopulationRuntime.hpp"
#include "pke/UiRenderer.hpp"
namespace pke {
enum class UiAction {None,Practice,Back,Side,Stars,Search,Song,Name,SaveName,SkinPrevious,SkinNext,Halloween,SaveReplay};
struct UiHit {UiAction action{UiAction::None};std::size_t index{};};
class ApplicationView {
public:
    explicit ApplicationView(UiRenderer& renderer):ui_(renderer){}
    void Prepare(const ApplicationFlow&);
    void Draw(const ApplicationFlow&,const PopulationSummary*,const PopulationStatus*,double animationMs);
    UiHit Hit(const ApplicationFlow&,double x,double y) const;
    void Scroll(const ApplicationFlow&,int rows);
    void EnsureSelected(const ApplicationFlow&);
    void GameplayOverlay();
    bool sideMenu{},starMenu{},searchFocus{},nameFocus{};
    bool replaySaved{};
    std::string nameEdit,composition,status,persistenceError;
private:
    void Button(std::string_view,std::string_view,SDL_Rect,UiColor,UiColor,int size=18);
    void Home(const ApplicationFlow&,const PopulationSummary*,const PopulationStatus*,double);
    void Practice(const ApplicationFlow&);
    void Result(const ApplicationFlow&);
    UiRenderer& ui_;int firstRow_{};
    std::string previewId_;
    const CharacterDefinition* preview_{};
    CharacterAnimator animator_;
};
}
