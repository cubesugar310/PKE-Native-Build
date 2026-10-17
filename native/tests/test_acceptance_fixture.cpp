#include "TestHarness.hpp"
#include "pke/ChartParser.hpp"
#include "pke/CharacterLoader.hpp"
#include "pke/GameSession.hpp"
#include <filesystem>
#include <fstream>
#ifndef PKE_TEST_FIXTURE_DIR
#define PKE_TEST_FIXTURE_DIR "."
#endif
namespace {
class AcceptanceClock : public pke::IAudioClock {
public:
    double t{}; pke::AudioState st{pke::AudioState::Ready};
    bool Start() override { st=pke::AudioState::Playing; return true; }
    void Pause() override { st=pke::AudioState::Paused; }
    void Resume() override { st=pke::AudioState::Playing; }
    void Stop() override { st=pke::AudioState::Stopped; }
    double SongTimeMs() const override { return t; }
    pke::AudioState State() const override { return st; }
    std::string LastErrorCopy() const override { return {}; }
};
SDL_Scancode laneKey(int lane){ static SDL_Scancode k[4]={SDL_SCANCODE_LEFT,SDL_SCANCODE_DOWN,SDL_SCANCODE_UP,SDL_SCANCODE_RIGHT}; return k[lane]; }
}
PKE_TEST("Real Ejected fixture plus real bfice metadata initialize and perfect playback completes") {
    const std::filesystem::path fixtures=PKE_TEST_FIXTURE_DIR;
    auto chart=pke::ParseChart(fixtures/"ejected-hard.json");
    PKE_REQUIRE(chart.notes.size()==1187);
    size_t playerCount=0; for(const auto& n:chart.notes) if(n.side==pke::Side::Player) ++playerCount;
    PKE_REQUIRE(playerCount==568);
    auto bfdir=fixtures/"character/bfice"; auto bf=pke::LoadCharacter({bfdir/"bfice.json",bfdir/"bfice.xml",{}}); PKE_REQUIRE(!bf.animations.at("idle").frameIndices.empty()); PKE_REQUIRE(!bf.atlas.empty());

    auto root=std::filesystem::temp_directory_path()/"pke-native-acceptance"; std::filesystem::remove_all(root); auto songDir=root/"Songs/ejected"; std::filesystem::create_directories(songDir); std::filesystem::copy_file(fixtures/"ejected-hard.json",songDir/"ejected-hard.json",std::filesystem::copy_options::overwrite_existing); std::ofstream(songDir/"Inst.ogg").put('x');
    pke::Config cfg; cfg.songsRoot="Songs"; cfg.charactersRoot="Characters"; AcceptanceClock clock; pke::GameSession session(cfg,root,&clock); PKE_REQUIRE(session.Load({"ejected","hard"})); session.Update(); session.Update(); PKE_REQUIRE(session.State()==pke::SessionState::Playing);
    double last=0; for(const auto& n:chart.notes){ if(n.side!=pke::Side::Player) continue; clock.t=n.timeMs; session.Update(); session.HandleKeyDown(laneKey(n.lane)); last=std::max(last,n.timeMs+n.sustainMs); }
    clock.t=last+250; session.Update(); clock.st=pke::AudioState::Finished; session.Update();
    PKE_REQUIRE(session.State()==pke::SessionState::Completed); PKE_REQUIRE(session.Stats().miss==0); PKE_REQUIRE(session.Stats().sick==568); PKE_REQUIRE(session.Stats().rawScore==568*350);
}
