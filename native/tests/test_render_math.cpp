#include "TestHarness.hpp"
#include "pke/Renderer.hpp"
PKE_TEST("Renderer letterboxes 1920x1080 into arbitrary output") {
    auto a=pke::ComputeLetterboxedViewport(1920,1080); PKE_REQUIRE(a.destination.x==0&&a.destination.y==0&&a.destination.w==1920&&a.destination.h==1080); PKE_REQUIRE_NEAR(a.scale,1.0,1e-9);
    auto b=pke::ComputeLetterboxedViewport(1280,1024); PKE_REQUIRE(b.destination.w==1280); PKE_REQUIRE(b.destination.h==720); PKE_REQUIRE(b.destination.y==152); PKE_REQUIRE_NEAR(b.scale,2.0/3.0,1e-9);
    auto c=pke::ComputeLetterboxedViewport(2560,1080); PKE_REQUIRE(c.destination.w==1920); PKE_REQUIRE(c.destination.h==1080); PKE_REQUIRE(c.destination.x==320);
}
PKE_TEST("Note Y is pure function of note-song delta and scroll speed") {
    PKE_REQUIRE_NEAR(pke::ComputeNoteY(2000,1000,2.0,200),1100,1e-9);
    PKE_REQUIRE_NEAR(pke::ComputeNoteY(2000,1500,2.0,200),650,1e-9);
    PKE_REQUIRE_NEAR(pke::ComputeNoteY(2000,1000,1.0,200),650,1e-9);
}
PKE_TEST("Render math does not mutate rhythm state") {
    pke::Chart c; c.notes.push_back({1000,0,0,pke::Side::Player,"",false,false}); pke::RhythmEngine r; r.Reset(c); auto before=r.Stats().rawScore; (void)pke::ComputeNoteY(c.notes[0].timeMs,500,3.0,200); PKE_REQUIRE(r.Stats().rawScore==before); PKE_REQUIRE(r.NoteStates()[0].status==pke::NoteStatus::Pending);
}
PKE_TEST("Renderer note visitor includes opponent chart notes and player runtime notes without duplication") {
    pke::Chart chart;
    chart.notes.push_back({1000,0,0,pke::Side::Opponent,"",false,false});
    chart.notes.push_back({1100,1,0,pke::Side::Player,"",false,false});
    pke::RhythmEngine rhythm; rhythm.Reset(chart);
    pke::RenderState rs; rs.songTimeMs=900; rs.scrollSpeed=1.0; rs.chart=&chart; rs.notes=&rhythm.NoteStates();
    int opponent=0, player=0;
    pke::ForEachRenderableNote(rs,[&](const pke::Note& n){ if(n.side==pke::Side::Opponent) ++opponent; else ++player; });
    PKE_REQUIRE(opponent==1);
    PKE_REQUIRE(player==1);
}
PKE_TEST("Renderer uses diagnostic fallback when a character texture is unavailable") {
    pke::CharacterDefinition def;
    def.atlas.push_back({"fallback",{0,0,64,64},0,0,64,64,false});
    pke::CharacterRenderView view{"missing",&def,{0,"idle",0.0},{100,200}};
    PKE_REQUIRE(pke::NeedsDiagnosticFallback(view,false));
    PKE_REQUIRE(!pke::NeedsDiagnosticFallback(view,true));
}
