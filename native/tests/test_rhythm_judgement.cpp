#include "TestHarness.hpp"
#include "pke/RhythmEngine.hpp"

namespace { pke::Chart chartAt(double t=1000){ pke::Chart c; c.notes.push_back({t,0,0,pke::Side::Player,"",false,false}); return c; } }
PKE_TEST("Rhythm exact judgement boundaries match V1.14") {
    struct C{double off; pke::Judgement j; int score; double health;};
    const C cases[]={{45,pke::Judgement::Sick,350,1.04},{-45,pke::Judgement::Sick,350,1.04},{45.001,pke::Judgement::Good,200,1.025},{90,pke::Judgement::Good,200,1.025},{90.001,pke::Judgement::Bad,100,1.01},{135,pke::Judgement::Bad,100,1.01}};
    for(auto x:cases){ pke::RhythmEngine r; auto c=chartAt(); r.Reset(c); auto got=r.KeyDown(0,1000+x.off); PKE_REQUIRE(got && *got==x.j); auto s=r.Stats(); PKE_REQUIRE(s.rawScore==x.score); PKE_REQUIRE_NEAR(s.health,x.health,1e-9); }
}
PKE_TEST("Rhythm outside BAD is not hit and reaches 180ms MISS deadline") {
    pke::RhythmEngine r; auto c=chartAt(); r.Reset(c); auto got=r.KeyDown(0,1135.001); PKE_REQUIRE(!got); r.Update(1179.999); PKE_REQUIRE(r.Stats().miss==0); r.Update(1180.0); auto s=r.Stats(); PKE_REQUIRE(s.miss==1); PKE_REQUIRE(s.rawScore==0); PKE_REQUIRE(s.combo==0);
}
PKE_TEST("Rhythm accuracy is deterministic and zero-safe") {
    pke::RhythmEngine r; auto c=chartAt(); r.Reset(c); PKE_REQUIRE_NEAR(r.Stats().accuracyPercent,100.0,1e-9); r.KeyDown(0,1090); PKE_REQUIRE_NEAR(r.Stats().accuracyPercent,200.0/350.0*100.0,1e-9);
}
