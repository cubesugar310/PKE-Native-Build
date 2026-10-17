#include "TestHarness.hpp"
#include "pke/ApplicationFlow.hpp"
#include <chrono>
#include <fstream>

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
