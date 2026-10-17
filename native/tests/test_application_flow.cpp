#include "TestHarness.hpp"
#include "pke/ApplicationFlow.hpp"
#include <chrono>
#include <fstream>
#include <functional>

namespace {
struct Library {
    std::filesystem::path root=std::filesystem::temp_directory_path()/("pke-flow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    pke::Config config;
    Library(){config.songsRoot="Songs";config.charactersRoot="Characters";
        for(const auto* id:{"one","two"}){const auto dir=root/"Songs"/id;std::filesystem::create_directories(dir);
            std::ofstream(dir/"Inst.ogg")<<"audio";std::ofstream(dir/"chart.json")<<"{\"name\":\""<<id<<"\",\"notes\":[{\"time\":1000,\"lane\":0},{\"time\":1500,\"lane\":2,\"side\":\"dad\"}]}";}}
    ~Library(){std::filesystem::remove_all(root);}
};
}
PKE_TEST("Application traverses Home Practice Gameplay Result and back without changing Online progress") {
    Library l;pke::AssetService assets(l.config,l.root,PKE_COMPAT_DIR,"en-US");assets.Refresh();
    pke::NativeSaveSummary player;player.valid=true;player.playerName="Existing";player.progress.rp=2400;player.progress.wins=42;
    pke::ApplicationFlow flow(assets,player);PKE_REQUIRE(flow.Screen()==pke::AppScreen::Home);
    flow.OpenPractice();PKE_REQUIRE(flow.VisibleSongs().size()==2);PKE_REQUIRE(flow.SelectedSong()==std::optional<std::size_t>{0});
    flow.MoveSelection(-1);PKE_REQUIRE(flow.SelectedSong()==std::optional<std::size_t>{0});
    flow.SetSide(pke::Side::Opponent);flow.MoveSelection(1);PKE_REQUIRE(flow.SelectedSong()==std::optional<std::size_t>{1});
    PKE_REQUIRE(flow.BeginGameplay());PKE_REQUIRE(flow.Screen()==pke::AppScreen::Gameplay);
    pke::PlayStats stats;stats.sick=2;stats.rawScore=700;stats.accuracyPercent=100;
    flow.FinishGameplay(stats);PKE_REQUIRE(flow.Screen()==pke::AppScreen::Result);PKE_REQUIRE(flow.Result().stats.sick==2);
    PKE_REQUIRE(flow.Result().songName=="two");PKE_REQUIRE(flow.Player().progress.rp==2400);PKE_REQUIRE(flow.Player().progress.wins==42);
    flow.Back();PKE_REQUIRE(flow.Screen()==pke::AppScreen::Practice);PKE_REQUIRE(flow.SelectedSong()==std::optional<std::size_t>{1});
    flow.Back();PKE_REQUIRE(flow.Screen()==pke::AppScreen::Home);
}
PKE_TEST("Practice filtering preserves source selection and cannot launch an empty result") {
    Library l;pke::AssetService assets(l.config,l.root,PKE_COMPAT_DIR,"en-US");assets.Refresh();pke::ApplicationFlow flow(assets,{});
    flow.OpenPractice();flow.SetSearch("ＴＷＯ");PKE_REQUIRE(flow.SelectedSong()==std::optional<std::size_t>{1});
    flow.SetStars(7);PKE_REQUIRE(flow.VisibleSongs().empty());PKE_REQUIRE(!flow.BeginGameplay());
    flow.SetStars(1);PKE_REQUIRE(flow.VisibleSongs()==std::vector<std::size_t>{1});
    flow.SetSearch("");flow.SelectSong(0);PKE_REQUIRE(flow.BeginGameplay());flow.Back();PKE_REQUIRE(flow.Screen()==pke::AppScreen::Practice);
    PKE_REQUIRE(flow.Player().progress.matches==0);
}

PKE_TEST("Halloween uses JST boundaries and gates stored overrides behind Developer authentication") {
    Library l;pke::AssetService assets(l.config,l.root,PKE_COMPAT_DIR,"en-US");assets.Refresh();
    pke::NativeSaveSummary player;player.halloweenDev.eventOverride="force-on";pke::ApplicationFlow flow(assets,player);
    using namespace std::chrono;
    const auto start=duration_cast<milliseconds>((sys_days{2026y/October/14}+15h).time_since_epoch()).count();
    const auto end=duration_cast<milliseconds>((sys_days{2026y/October/31}+15h).time_since_epoch()).count();
    PKE_REQUIRE(!flow.Halloween(start-1).active);PKE_REQUIRE(flow.Halloween(start).active);
    PKE_REQUIRE(flow.Halloween(end-1).active);PKE_REQUIRE(!flow.Halloween(end).active);
    PKE_REQUIRE(!flow.EnableDeveloper("wrong","wrong"));PKE_REQUIRE(!flow.Halloween(end).active);
    PKE_REQUIRE(flow.EnableDeveloper("cubesugar","A38UVGD6"));PKE_REQUIRE(flow.Halloween(end).active);
    player.halloweenDev.eventOverride="force-off";pke::ApplicationFlow off(assets,player);
    PKE_REQUIRE(off.Halloween(start).active);off.EnableDeveloper("cubesugar","A38UVGD6");PKE_REQUIRE(!off.Halloween(start).active);
}

PKE_TEST("Halloween purchases commit only after durable save and Practice never awards Candy") {
    Library l;pke::AssetService assets(l.config,l.root,PKE_COMPAT_DIR,"en-US");assets.Refresh();
    pke::NativeSaveSummary player;player.valid=true;player.halloweenCandy=8000;player.progress.rp=4321;
    pke::ApplicationFlow flow(assets,player);flow.EnableDeveloper("cubesugar","A38UVGD6");
    const auto now=1792000000000LL;int writes=0;
    auto fail=[&](const auto&){++writes;return false;};
    auto save=[&](const auto& candidate){++writes;PKE_REQUIRE(candidate.halloweenCandy==3000);return true;};
    PKE_REQUIRE(flow.PurchaseHalloweenTitle(now,fail)=="error");PKE_REQUIRE(flow.Player().halloweenCandy==8000);
    PKE_REQUIRE(flow.Player().halloweenHistory.empty());
    PKE_REQUIRE(flow.PurchaseHalloweenTitle(now,save)=="purchased");PKE_REQUIRE(flow.Player().halloweenCandy==3000);
    PKE_REQUIRE(flow.Player().halloweenHistory.at("2026").purchases==std::vector<std::string>{"trick-or-beat"});
    PKE_REQUIRE(flow.PurchaseHalloweenTitle(now,save)=="owned");PKE_REQUIRE(writes==2);
    flow.OpenPractice();PKE_REQUIRE(flow.BeginGameplay());flow.FinishGameplay({});PKE_REQUIRE(flow.Player().halloweenCandy==3000);
    PKE_REQUIRE(flow.Player().progress.rp==4321);
    player.halloweenCandy=4999;pke::ApplicationFlow poor(assets,player);
    PKE_REQUIRE(poor.PurchaseHalloweenTitle(now,save)=="insufficient");PKE_REQUIRE(writes==2);
    PKE_REQUIRE(flow.HalloweenShop().empty());
}
PKE_TEST("Halloween shop accepts original directory-derived IDs and hides duplicate missing and unpurchased skins") {
    Library l;const auto dir=l.root/"Characters"/"holiday";std::filesystem::create_directories(dir);
    std::ofstream(dir/"bf.json")<<R"({"image":"bf","animations":[{"anim":"idle","name":"idle"}]})";
    std::ofstream(dir/"bf.xml")<<R"(<TextureAtlas imagePath="bf.png"><SubTexture name="idle0000" x="0" y="0" width="32" height="32"/></TextureAtlas>)";
    std::ofstream(dir/"bf.png")<<"cached image path";
    pke::AssetService assets(l.config,l.root,PKE_COMPAT_DIR,"en-US");assets.Refresh();
    pke::NativeSaveSummary player;player.halloweenCandy=4000;player.halloweenDev.skinSlots[0]={"sparrow:holiday",3000};
    player.halloweenDev.skinSlots[1]={"sparrow:holiday",1};player.halloweenDev.skinSlots[2]={"sparrow:missing",1};
    pke::ApplicationFlow flow(assets,player);const auto shop=flow.HalloweenShop();PKE_REQUIRE(shop.size()==1);
    PKE_REQUIRE(shop[0].slotIndex==0);PKE_REQUIRE(!flow.SkinAvailable("bf"));flow.SetSkin("bf");PKE_REQUIRE(flow.Player().selectedSkin.empty());
    const auto now=1792000000000LL;int writes=0;auto save=[&](const auto&){++writes;return true;};
    PKE_REQUIRE(flow.PurchaseHalloweenSkin(1,now,save)=="unavailable");PKE_REQUIRE(writes==0);
    PKE_REQUIRE(flow.PurchaseHalloweenSkin(0,now,save)=="purchased");PKE_REQUIRE(flow.Player().halloweenCandy==1000);
    PKE_REQUIRE(flow.SkinAvailable("bf"));flow.SetSkin("bf");PKE_REQUIRE(flow.Player().selectedSkin=="bf");
    PKE_REQUIRE(flow.PurchaseHalloweenSkin(0,now,save)=="owned");PKE_REQUIRE(writes==1);
}
PKE_TEST("Empty selection chooses an entitled source skin and colliding directory IDs remain distinct") {
    Library l;const auto dir=l.root/"Characters"/"holiday";std::filesystem::create_directories(dir);
    for(const auto* id:{"alt","bf"}){std::ofstream(dir/(std::string(id)+".json"))<<"{\"image\":\""<<id<<"\",\"animations\":[{\"anim\":\"idle\",\"name\":\"idle\"}]}";
        std::ofstream(dir/(std::string(id)+".xml"))<<"<TextureAtlas imagePath=\""<<id<<".png\"><SubTexture name=\"idle0000\" x=\"0\" y=\"0\" width=\"32\" height=\"32\"/></TextureAtlas>";
        std::ofstream(dir/(std::string(id)+".png"))<<"image";}
    pke::AssetService assets(l.config,l.root,PKE_COMPAT_DIR,"en-US");const auto catalog=assets.Refresh();PKE_REQUIRE(catalog->characters.size()==2);
    PKE_REQUIRE(catalog->characters[0].sourceSkinId=="sparrow:holiday");PKE_REQUIRE(catalog->characters[1].sourceSkinId=="sparrow:holiday-2");
    pke::NativeSaveSummary player;player.halloweenDev.skinSlots[0]={"sparrow:holiday-2",3000};
    pke::ApplicationFlow flow(assets,player);PKE_REQUIRE(flow.Player().selectedSkin=="sparrow:holiday");PKE_REQUIRE(!flow.SkinAvailable("bf"));
    player.halloweenDev.skinSlots[1]={"sparrow:holiday",3000};pke::ApplicationFlow allGated(assets,player);
    PKE_REQUIRE(allGated.Player().selectedSkin.empty());PKE_REQUIRE(!allGated.SkinAvailable("alt"));
    const auto other=l.root/"Characters"/"holiday-2";std::filesystem::create_directories(other);
    std::filesystem::copy_file(dir/"alt.json",other/"extra.json");std::filesystem::copy_file(dir/"alt.xml",other/"alt.xml");
    std::filesystem::copy_file(dir/"alt.png",other/"alt.png");const auto refreshed=assets.Refresh();
    const auto* suffixed=pke::FindCharacterAsset(*refreshed,"sparrow:holiday-3");PKE_REQUIRE(suffixed&&suffixed->id=="bf");
    const auto* directory=pke::FindCharacterAsset(*refreshed,"sparrow:holiday-2");PKE_REQUIRE(directory&&directory->id=="extra");
}
PKE_TEST("Settings reject failed persistence and commit only validated gameplay preferences") {
    Library l;pke::AssetService assets(l.config,l.root,PKE_COMPAT_DIR,"en-US");assets.Refresh();pke::ApplicationFlow flow(assets,{});
    auto fail=[](const auto&){return false;};auto save=[](const auto&){return true;};
    PKE_REQUIRE(!flow.SavePreferences(0.3,"down","corner",fail));PKE_REQUIRE(flow.Player().masterVolume==1);
    PKE_REQUIRE(!flow.SavePreferences(0.3,"invalid","corner",save));
    PKE_REQUIRE(flow.SavePreferences(0.3,"down","corner",save));PKE_REQUIRE(flow.Player().scrollDirection=="down");
    PKE_REQUIRE(flow.Player().judgementPosition=="corner");PKE_REQUIRE_NEAR(flow.Player().masterVolume,0.3,1e-6);
}
