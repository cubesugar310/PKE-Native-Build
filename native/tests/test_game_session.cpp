#include "TestHarness.hpp"
#include "pke/GameSession.hpp"
#include "pke/QuickJsRuntime.hpp"
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

PKE_TEST("Practice loads cached source heads and the selected side without rereading chart files") {
    auto root=makeSong("pke-session-cached");
    std::ofstream(root/"Songs/demo/demo-hard.json")<<R"({"song":{"speed":2,"notes":[{"mustHitSection":true,"sectionNotes":[[1000,0,0],[1500,6,0],[1750,1,0,"Hurt Note"]]}]}})";
    pke::Config cfg;cfg.songsRoot="Songs";cfg.charactersRoot="Characters";
    pke::AssetService assets(cfg,root,PKE_COMPAT_DIR,"en-US");const auto catalog=assets.Refresh();
    PKE_REQUIRE(catalog->songs.size()==1);PKE_REQUIRE(catalog->songs[0].charts[0].chart.notes.size()==2);
    std::filesystem::remove_all(root);FakeClock clock;pke::GameSession session(cfg,root,&clock);
    PKE_REQUIRE(session.LoadPractice(catalog->songs[0],*catalog,pke::Side::Opponent,""));
    session.Update();session.Update();clock.t=1500;session.HandleKeyDown(SDL_SCANCODE_UP);
    PKE_REQUIRE(session.Stats().sick==1);clock.state=pke::AudioState::Finished;session.Update();
    PKE_REQUIRE(catalog->songs[0].charts[0].chart.notes[0].side==pke::Side::Player);
}

PKE_TEST("Finished Practice exports original replay schema and recorded ghost taps without changing stats") {
    auto root=makeSong("pke-session-replay");pke::Config cfg;cfg.songsRoot="Songs";cfg.charactersRoot="Characters";
    pke::AssetService assets(cfg,root,PKE_COMPAT_DIR,"en-US");const auto catalog=assets.Refresh();
    FakeClock clock;pke::GameSession session(cfg,root,&clock);
    PKE_REQUIRE(session.LoadPractice(catalog->songs[0],*catalog,pke::Side::Player,""));
    session.Update();session.Update();clock.t=1000;session.HandleKeyDown(SDL_SCANCODE_LEFT);
    clock.t=1020;session.HandleKeyUp(SDL_SCANCODE_LEFT);clock.t=1200;session.HandleKeyDown(SDL_SCANCODE_RIGHT);
    clock.t=1500;session.HandleKeyDown(SDL_SCANCODE_DOWN);clock.state=pke::AudioState::Finished;session.Update();
    const auto replay=session.ExportReplay(PKE_COMPAT_DIR);pke::QuickJsRuntime runtime;
    PKE_REQUIRE(runtime.Evaluate(R"(globalThis.check=r=>r.format==='PKE_REPLAY'&&r.schemaVersion===1&&
      r.settings.practiceReplay===true&&r.settings.beatBattleMode===false&&r.result.score===700&&
      r.result.sick===2&&r.result.accuracy===100&&r.result.fcStatus==='SFC'&&r.result.beatBattle===null&&
      r.inputs.length===4&&r.inputs[1].type==='up'&&r.inputs[2].lane===3&&
      r.chartSnapshot.notes.length===2&&r.chartSnapshot.notes[0].side==='player'&&
      /^fnv1a32-[0-9a-f]{8}$/.test(r.chartHash);)").ok);
    const auto result=runtime.CallJson("check",replay);PKE_REQUIRE(result.ok);PKE_REQUIRE(result.json=="true");
    PKE_REQUIRE(session.Stats().sick==2);
}
PKE_TEST("Input offset compensates automatic head misses in the same clock as key judgement") {
    const auto root=makeSong("pke-session-offset");pke::Config config;config.songsRoot="Songs";config.charactersRoot="Characters";
    for(const double offset:{100.0,-100.0}){config.inputOffsetMs=offset;FakeClock clock;pke::GameSession session(config,root,&clock);
        PKE_REQUIRE(session.Load({"demo","hard"}));session.Update();session.Update();
        clock.t=offset>0?1070:1200;session.Update();PKE_REQUIRE(session.Stats().miss==(offset>0?1:0));
        if(offset<0){session.HandleKeyDown(SDL_SCANCODE_LEFT);PKE_REQUIRE(session.Stats().bad==1);}}
}
PKE_TEST("Input offset does not shift the raw sustain release grace clock") {
    const auto root=makeSong("pke-session-offset-hold");
    std::ofstream(root/"Songs/demo/demo-hard.json")<<R"({"song":{"bpm":120,"notes":[{"mustHitSection":true,"sectionNotes":[[1000,0,1000]]}]}})";
    pke::Config config;config.songsRoot="Songs";config.charactersRoot="Characters";
    for(const double offset:{100.0,-100.0}){config.inputOffsetMs=offset;FakeClock clock;pke::GameSession session(config,root,&clock);
        PKE_REQUIRE(session.Load({"demo","hard"}));session.Update();session.Update();clock.t=1000-offset;session.HandleKeyDown(SDL_SCANCODE_LEFT);
        clock.t=1300;session.HandleKeyUp(SDL_SCANCODE_LEFT);clock.t=1390;session.Update();PKE_REQUIRE(session.Stats().miss==0);
        clock.t=1390.001;session.Update();PKE_REQUIRE(session.Stats().miss==1);}
}
