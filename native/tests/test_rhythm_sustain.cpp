#include "TestHarness.hpp"
#include "pke/RhythmEngine.hpp"
namespace { pke::Chart sustainChart(){ pke::Chart c; c.notes.push_back({1000,2,1000,pke::Side::Player,"",false,false}); return c; } }
PKE_TEST("Sustain completes when held through end independent of update cadence") {
    auto run=[](double step){ pke::RhythmEngine r; auto c=sustainChart(); r.Reset(c); r.KeyDown(2,1000); for(double t=1000;t<=2050;t+=step) r.Update(t); r.KeyUp(2,2000); r.Update(2100); return r.NoteStates().at(0).status; };
    PKE_REQUIRE(run(1000.0/60.0)==pke::NoteStatus::SustainCompleted); PKE_REQUIRE(run(1000.0/240.0)==pke::NoteStatus::SustainCompleted);
}
PKE_TEST("Sustain release grace uses release time and permits regrabbing before 90ms") {
    pke::RhythmEngine r;auto c=sustainChart();r.Reset(c);r.KeyDown(2,1000);r.Update(1000);r.KeyUp(2,1300);r.Update(1390);
    PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::Holding);r.KeyDown(2,1390);r.Update(2000);
    PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::SustainCompleted);PKE_REQUIRE(r.Stats().miss==0);
}
PKE_TEST("Late accepted sustain head stays held and a real break applies one original hold miss") {
    pke::RhythmEngine r;auto c=sustainChart();r.Reset(c);r.KeyDown(2,1090);r.Update(1090);PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::Holding);
    r.KeyUp(2,1300);r.Update(1390.001);PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::SustainBroken);
    PKE_REQUIRE(r.Stats().miss==1);PKE_REQUIRE(r.Stats().rawScore==190);PKE_REQUIRE(r.Stats().combo==0);
    PKE_REQUIRE_NEAR(r.Stats().accuracyPercent,75,1e-9);r.Update(2200);PKE_REQUIRE(r.Stats().miss==1);
}
PKE_TEST("Original sustain children add HP once without inflating head score or accuracy") {
    pke::RhythmEngine r;auto c=sustainChart();c.baseBpm=120;c.bpmTimeline={{0,120}};r.Reset(c);r.KeyDown(2,1000);r.Update(2000);
    PKE_REQUIRE(r.Stats().rawScore==350);PKE_REQUIRE(r.Stats().sick==1);PKE_REQUIRE_NEAR(r.Stats().accuracyPercent,100,1e-9);
    PKE_REQUIRE_NEAR(r.Stats().health,1.04+8*.006,1e-9);
}
PKE_TEST("Early sustain release retains start grace and expired regrabs cannot erase a break") {
    pke::RhythmEngine r;auto c=sustainChart();r.Reset(c);r.KeyDown(2,955);r.KeyUp(2,960);r.Update(1051);
    PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::Holding);r.Update(1150);
    PKE_REQUIRE(r.Stats().miss==0);r.KeyDown(2,1150.001);PKE_REQUIRE(r.Stats().miss==1);
    r.Reset(c);r.KeyDown(2,955);r.KeyUp(2,960);r.KeyDown(2,1100);r.Update(2000);PKE_REQUIRE(r.Stats().miss==0);
}
