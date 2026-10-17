#include "TestHarness.hpp"
#include "pke/RhythmEngine.hpp"

namespace { pke::Chart chartAt(double t=1000){ pke::Chart c; c.notes.push_back({t,0,0,pke::Side::Player,"",false,false}); return c; } }
PKE_TEST("Practice authored head windows match extracted source judgeLaneAt") {
    struct C{double off; pke::Judgement j; int score; double health;};
    const C cases[]={{45,pke::Judgement::Sick,350,1.04},{-45,pke::Judgement::Sick,350,1.04},{45.001,pke::Judgement::Good,200,1.025},{90,pke::Judgement::Good,200,1.025},{90.001,pke::Judgement::Bad,100,1.01},{135,pke::Judgement::Bad,100,1.01},{165,pke::Judgement::Bad,100,1.01}};
    for(auto x:cases){ pke::RhythmEngine r; auto c=chartAt(); r.Reset(c); auto got=r.KeyDown(0,1000+x.off); PKE_REQUIRE(got && *got==x.j); auto s=r.Stats(); PKE_REQUIRE(s.rawScore==x.score); PKE_REQUIRE_NEAR(s.health,x.health,1e-9); }
}
PKE_TEST("Practice applies the source bootstrap ten-safe-frame miss deadline and original loss") {
    const double cutoff=10*(1000.0/60.0);auto c=chartAt(0);pke::RhythmEngine r;r.Reset(c);
    PKE_REQUIRE(r.KeyDown(0,cutoff)==pke::Judgement::Bad);r.Reset(c);
    PKE_REQUIRE(!r.KeyDown(0,cutoff+.001));r.Update(cutoff);PKE_REQUIRE(r.Stats().miss==0);
    r.Update(cutoff+.001);auto s=r.Stats();PKE_REQUIRE(s.miss==1);PKE_REQUIRE(s.rawScore==-10);PKE_REQUIRE(s.combo==0);PKE_REQUIRE_NEAR(s.health,.88,1e-9);
}
PKE_TEST("Rhythm accuracy is deterministic and zero-safe") {
    pke::RhythmEngine r; auto c=chartAt(); r.Reset(c); PKE_REQUIRE_NEAR(r.Stats().accuracyPercent,0.0,1e-9); r.KeyDown(0,1090); PKE_REQUIRE_NEAR(r.Stats().accuracyPercent,75.0,1e-9);
}
PKE_TEST("Practice one input judges original stacked authored heads on its lane") {
    auto c=chartAt();c.notes.push_back({1000.005,0,0,pke::Side::Player});c.notes.push_back({1000,1,0,pke::Side::Player});
    pke::RhythmEngine r;r.Reset(c);r.KeyDown(0,1000);PKE_REQUIRE(r.Stats().sick==2);PKE_REQUIRE(r.Stats().combo==2);PKE_REQUIRE(r.Stats().rawScore==700);
    PKE_REQUIRE(r.NoteStates()[2].status==pke::NoteStatus::Pending);
}
