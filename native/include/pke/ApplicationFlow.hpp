#pragma once
#include "pke/AssetService.hpp"
#include "pke/LegacySaveImporter.hpp"
#include "pke/RhythmEngine.hpp"
#include <functional>

namespace pke {
enum class AppScreen {Home,Practice,Gameplay,Result,Halloween,Settings,Developer};
struct HalloweenWindow {bool active{};int year{};std::int64_t remainingMs{};};
struct HalloweenShopEntry {std::size_t slotIndex{};std::string skinId;int price{};const CharacterAsset* character{};};
struct PracticeResult {std::string songName;PlayStats stats;Side side{Side::Player};};
class ApplicationFlow {
public:
    ApplicationFlow(AssetService&,NativeSaveSummary);
    AppScreen Screen() const noexcept{return screen_;}
    const NativeSaveSummary& Player() const noexcept{return player_;}
    const PracticeResult& Result() const noexcept{return result_;}
    const std::vector<std::size_t>& VisibleSongs() const noexcept{return visible_;}
    std::optional<std::size_t> SelectedSong() const noexcept{return selected_;}
    std::shared_ptr<const AssetCatalog> Catalog() const noexcept{return catalog_;}
    Side PracticeSide() const noexcept{return side_;}
    int Stars() const noexcept{return stars_;}
    const std::string& Search() const noexcept{return search_;}
    void OpenPractice();
    void OpenHalloween();
    void OpenSettings(){if(screen_==AppScreen::Home)screen_=AppScreen::Settings;}
    void OpenDeveloper(){if(screen_==AppScreen::Home)screen_=AppScreen::Developer;}
    HalloweenWindow Halloween(std::int64_t nowMs) const;
    std::vector<HalloweenShopEntry> HalloweenShop() const;
    bool SkinAvailable(std::string_view) const;
    bool EnableDeveloper(std::string_view code,std::string_view pin);
    bool DeveloperEnabled() const noexcept{return developerEnabled_;}
    using PersistPlayer=std::function<bool(const NativeSaveSummary&)>;
    std::string PurchaseHalloweenTitle(std::int64_t,const PersistPlayer&);
    std::string PurchaseHalloweenSkin(std::size_t,std::int64_t,const PersistPlayer&);
    bool SavePreferences(double,std::string,std::string,const PersistPlayer&);
    bool SaveEventOverride(std::string,const PersistPlayer&);
    void Back();
    void SetSearch(std::string);
    void SetStars(int);
    void SetSide(Side);
    void SelectSong(std::size_t);
    void MoveSelection(int);
    bool BeginGameplay();
    void FinishGameplay(const PlayStats&);
    void SetPlayerName(std::string);
    void SetSkin(std::string);
    void RefreshCatalog();
private:
    void Filter();
    AssetService& assets_;
    NativeSaveSummary player_;
    std::shared_ptr<const AssetCatalog> catalog_;
    AppScreen screen_{AppScreen::Home};
    Side side_{Side::Player};int stars_{};
    std::string search_;
    std::vector<std::size_t> visible_;
    std::optional<std::size_t> selected_;
    PracticeResult result_;
    bool developerEnabled_{};
};
}
