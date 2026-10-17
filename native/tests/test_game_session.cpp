#include "TestHarness.hpp"
#include "pke/GameSession.hpp"
#include <filesystem>
#include <fstream>

namespace {
class FakeClock : public pke::IAudioClock {
public:
    double t{0}; pke::AudioState state{pke::AudioState::Ready}; std::string err;
    bool Start() override { state=pke::AudioState::Playing; return true; }
    void Pause() override { state=pke::AudioState::Paused; }
    void Resume() override { state=pke::AudioState::Playing; }
    void Stop() override { state=pke::AudioState::Stopped; }
    double SongTimeMs() const override { return t; }
    pke::AudioState State() const override { return state; }
    std::string LastErrorCopy() const override { return err; }
};
std::filesystem::path makeSong(const char* tag,bool voices=false){
    auto root=std::filesystem::temp_directory_path()/tag; std::filesystem::remove_all(root); auto f=root/"Songs"/"demo"; std::filesystem::create_directories(f); std::ofstream(f/"Inst.ogg").put('\0'); if(voices)std::ofstream(f/"Voices.ogg").put('\0');
    std::ofstream(f/"demo-hard.json") << R"({"song":{"song":"Demo","bpm":120,"speed":2,"player1":"missing-bf","player2":"missing-dad","notes":[{"lengthInSteps":16,"mustHitSection":true,"sectionNotes":[[1000,0,0],[1500,1,0]]}]}})"; return root;
}
}
PKE_TEST("GameSession lifecycle completes with fake audio clock") {
    auto root=makeSong("pke-session-life"); pke::Config cfg; cfg.songsRoot="Songs"; cfg.charactersRoot="Characters"; FakeClock clock; pke::GameSession s(cfg,root,&clock);
    PKE_REQUIRE(s.Load({"demo","hard"})); PKE_REQUIRE(s.State()==pke::SessionState::Ready); s.Update(); PKE_REQUIRE(s.State()==pke::SessionState::Countdown); s.Update(); PKE_REQUIRE(s.State()==pke::SessionState::Playing);
    clock.t=1000; s.HandleKeyDown(SDL_SCANCODE_LEFT); clock.t=1500; s.HandleKeyDown(SDL_SCANCODE_DOWN); clock.state=pke::AudioState::Finished; s.Update(); PKE_REQUIRE(s.State()==pke::SessionState::Completed); PKE_REQUIRE(s.Stats().sick==2);
}
PKE_TEST("GameSession judgement state is same under 60 and 240 Hz updates") {
    auto run=[](double dt){ auto root=makeSong(dt<5?"pke-session-240":"pke-session-60"); pke::Config cfg; cfg.songsRoot="Songs"; cfg.charactersRoot="Characters"; FakeClock c; pke::GameSession s(cfg,root,&c); PKE_REQUIRE(s.Load({"demo","hard"})); s.Update(); s.Update(); double nextHit1=1000,nextHit2=1500; bool h1=false,h2=false; for(double t=0;t<1900;t+=dt){ c.t=t; s.Update(); if(!h1 && t>=nextHit1){ c.t=nextHit1; s.HandleKeyDown(SDL_SCANCODE_LEFT); h1=true; } if(!h2&&t>=nextHit2){ c.t=nextHit2; s.HandleKeyDown(SDL_SCANCODE_DOWN); h2=true; }} return s.Stats(); };
    auto a=run(1000.0/60.0),b=run(1000.0/240.0); PKE_REQUIRE(a.rawScore==b.rawScore); PKE_REQUIRE(a.sick==b.sick); PKE_REQUIRE(a.miss==b.miss); PKE_REQUIRE_NEAR(a.accuracyPercent,b.accuracyPercent,1e-9);
}
PKE_TEST("GameSession records fixed-size frame diagnostics and toggles overlay") {
    auto root=makeSong("pke-session-diag"); pke::Config cfg; cfg.songsRoot="Songs"; cfg.charactersRoot="Characters"; FakeClock clock; pke::GameSession s(cfg,root,&clock);
    PKE_REQUIRE(s.Load({"demo","hard"}));
    PKE_REQUIRE(!s.DiagnosticsVisible());
    s.ToggleDiagnostics();
    PKE_REQUIRE(s.DiagnosticsVisible());
    s.RecordFrameTiming(16.7,0.2,0.4);
    auto d=s.DiagnosticsSummaryCopy();
    PKE_REQUIRE(d.frameSamples==1);
    PKE_REQUIRE_NEAR(d.latestFrame.intervalMs,16.7,1e-9);
}
PKE_TEST("GameSession destructor stops the active audio clock before owned diagnostics disappear") {
    auto root=makeSong("pke-session-dtor-stop"); pke::Config cfg; cfg.songsRoot="Songs"; cfg.charactersRoot="Characters"; FakeClock clock;
    {
        pke::GameSession s(cfg,root,&clock);
        PKE_REQUIRE(s.Load({"demo","hard"}));
        s.Update(); s.Update();
        PKE_REQUIRE(clock.state==pke::AudioState::Playing);
    }
    PKE_REQUIRE(clock.state==pke::AudioState::Stopped);
}
