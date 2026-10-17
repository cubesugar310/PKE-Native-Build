#include "TestHarness.hpp"
#include "pke/ChartParser.hpp"
#include <filesystem>
#include <fstream>

namespace { std::filesystem::path writeJson(const char* name,const std::string& s){ auto p=std::filesystem::temp_directory_path()/name; std::ofstream(p)<<s; return p; } }

PKE_TEST("Chart parser maps mustHit and noteData side exactly once") {
    auto p=writeJson("pke-chart-map.json",R"({"song":{"bpm":120,"speed":2.5,"player1":"bf","player2":"dad","notes":[{"lengthInSteps":16,"mustHitSection":true,"sectionNotes":[[100,0,0],[100,4,25,"Mine"]]},{"lengthInSteps":16,"mustHitSection":false,"sectionNotes":[[200,1,0],[200,5,0]]}]}})");
    auto c=pke::ParseChart(p);
    PKE_REQUIRE(c.notes.size()==4);
    PKE_REQUIRE(c.notes[0].lane==0 && c.notes[0].side==pke::Side::Player);
    PKE_REQUIRE(c.notes[1].lane==0 && c.notes[1].side==pke::Side::Opponent);
    PKE_REQUIRE(c.notes[1].sustainMs==25 && c.notes[1].noteType=="Mine");
    PKE_REQUIRE(c.notes[2].lane==1 && c.notes[2].side==pke::Side::Opponent);
    PKE_REQUIRE(c.notes[3].lane==1 && c.notes[3].side==pke::Side::Player);
}

PKE_TEST("Chart parser builds BPM timeline and deterministic scroll speed") {
    auto p=writeJson("pke-chart-bpm.json",R"({"scrollSpeed":9,"song":{"bpm":120,"speed":2,"scrollSpeed":3,"notes":[{"lengthInSteps":16,"mustHitSection":false,"sectionNotes":[]},{"lengthInSteps":16,"mustHitSection":false,"changeBPM":true,"bpm":240,"sectionNotes":[]}]}})");
    auto c=pke::ParseChart(p);
    PKE_REQUIRE_NEAR(c.baseBpm,120,1e-9); PKE_REQUIRE_NEAR(c.scrollSpeed,3,1e-9);
    PKE_REQUIRE(c.bpmTimeline.size()==2); PKE_REQUIRE_NEAR(c.bpmTimeline[1].timeMs,2000,1e-6); PKE_REQUIRE_NEAR(c.bpmTimeline[1].bpm,240,1e-9);
}

PKE_TEST("Chart parser malformed input mentions path") {
    auto p=writeJson("pke-chart-bad.json","{not json"); bool threw=false;
    try { (void)pke::ParseChart(p); } catch(const std::exception& e){ threw=true; PKE_REQUIRE(std::string(e.what()).find(p.string())!=std::string::npos); }
    PKE_REQUIRE(threw);
}

PKE_TEST("Chart parser warns for unsupported events instead of crashing") {
    auto p=writeJson("pke-chart-events.json",R"({"song":{"bpm":100,"events":[[1,[["Change Character","0","bf"]]]],"notes":[]}})");
    auto c=pke::ParseChart(p); PKE_REQUIRE(!c.warnings.empty());
}
