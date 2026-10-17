#include "TestHarness.hpp"
#include "pke/RhythmEngine.hpp"
namespace { pke::Chart sustainChart(){ pke::Chart c; c.notes.push_back({1000,2,1000,pke::Side::Player,"",false,false}); return c; } }
PKE_TEST("Sustain completes when held through end independent of update cadence") {
    auto run=[](double step){ pke::RhythmEngine r; auto c=sustainChart(); r.Reset(c); r.KeyDown(2,1000); for(double t=1000;t<=2050;t+=step) r.Update(t); r.KeyUp(2,2000); r.Update(2100); return r.NoteStates().at(0).status; };
    PKE_REQUIRE(run(1000.0/60.0)==pke::NoteStatus::SustainCompleted); PKE_REQUIRE(run(1000.0/240.0)==pke::NoteStatus::SustainCompleted);
}
PKE_TEST("Sustain release grace accepts within 90ms and breaks outside") {
    { pke::RhythmEngine r; auto c=sustainChart(); r.Reset(c); r.KeyDown(2,1000); r.KeyUp(2,1910); r.Update(2100); PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::SustainCompleted); }
    { pke::RhythmEngine r; auto c=sustainChart(); r.Reset(c); r.KeyDown(2,1000); r.KeyUp(2,1909.9); r.Update(2100); PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::SustainBroken); }
}
PKE_TEST("Sustain requires hold start within 60ms grace") {
    { pke::RhythmEngine r; auto c=sustainChart(); r.Reset(c); auto j=r.KeyDown(2,1060); PKE_REQUIRE(j.has_value()); PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::Holding); }
    { pke::RhythmEngine r; auto c=sustainChart(); r.Reset(c); auto j=r.KeyDown(2,1060.1); PKE_REQUIRE(j.has_value()); PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::SustainBroken); }
}
