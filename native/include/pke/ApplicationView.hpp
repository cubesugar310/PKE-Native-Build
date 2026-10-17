#pragma once
#include "pke/ApplicationFlow.hpp"
#include "pke/PopulationRuntime.hpp"
#include "pke/UiRenderer.hpp"
namespace pke {
enum class UiAction {None,Practice,Back,Side,Stars,Search,Song,Name,SaveName,SkinPrevious,SkinNext,Halloween,SaveReplay,ShopTab,HistoryTab,BuyTitle,BuySkin,Settings,VolumeDown,VolumeUp,ScrollDirection,JudgementPosition,Developer,DeveloperAuthenticate,EventOverride};
struct UiHit {UiAction action{UiAction::None};std::size_t index{};};
class ApplicationView {
public:
    explicit ApplicationView(UiRenderer& renderer):ui_(renderer){}
    void Prepare(const ApplicationFlow&);
    void Draw(const ApplicationFlow&,const PopulationSummary*,const PopulationStatus*,double animationMs);
    UiHit Hit(const ApplicationFlow&,double x,double y) const;
    void Scroll(const ApplicationFlow&,int rows);
    void EnsureSelected(const ApplicationFlow&);
    void GameplayOverlay(bool downScroll=false);
    void SetWallClock(std::int64_t now){now_=now;}
    bool sideMenu{},starMenu{},searchFocus{},nameFocus{};
    bool replaySaved{};
    std::string nameEdit,composition,status,persistenceError;
    bool historyTab{},developerPinStage{};
    std::string developerInput,developerCode;
private:
    void Button(std::string_view,std::string_view,SDL_Rect,UiColor,UiColor,int size=18);
    void Home(const ApplicationFlow&,const PopulationSummary*,const PopulationStatus*,double);
    void Practice(const ApplicationFlow&);
    void Result(const ApplicationFlow&);
    void Halloween(const ApplicationFlow&);
    void Settings(const ApplicationFlow&);
    void Developer(const ApplicationFlow&);
    UiRenderer& ui_;int firstRow_{};
    std::string previewId_;
    const CharacterDefinition* preview_{};
    CharacterAnimator animator_;
    std::vector<HalloweenShopEntry> shop_;
    std::int64_t now_{};
};
}
