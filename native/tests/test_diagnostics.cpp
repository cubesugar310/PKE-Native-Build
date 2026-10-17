#include "TestHarness.hpp"
#include "pke/Diagnostics.hpp"
PKE_TEST("Diagnostics fixed ring overwrites oldest and snapshots chronologically") {
    pke::Diagnostics d(3,2,2); d.RecordFrame({1,2,3,10}); d.RecordFrame({2,3,4,20}); d.RecordFrame({3,4,5,30}); d.RecordFrame({4,5,6,40});
    auto s=d.Snapshot(); PKE_REQUIRE(s.frames.size()==3); PKE_REQUIRE_NEAR(s.frames[0].songTimeMs,20,1e-9); PKE_REQUIRE_NEAR(s.frames[2].songTimeMs,40,1e-9);
    d.RecordAudio({10,256,false}); d.RecordAudio({20,256,true}); d.RecordAudio({30,128,false}); s=d.Snapshot(); PKE_REQUIRE(s.audio.size()==2); PKE_REQUIRE(s.audio[0].songFrame==20); PKE_REQUIRE(s.audio[1].songFrame==30);
}
PKE_TEST("Diagnostics reset clears counts without changing capacity") { pke::Diagnostics d(2,2,2); d.RecordInputOffset(12.5); d.RecordFrame({}); d.Reset(); auto s=d.Snapshot(); PKE_REQUIRE(s.frames.empty()); PKE_REQUIRE(s.audio.empty()); PKE_REQUIRE(s.inputOffsetsMs.empty()); d.RecordInputOffset(-1); PKE_REQUIRE(d.Snapshot().inputOffsetsMs.size()==1); }
PKE_TEST("Diagnostics summary exposes latest samples without snapshot vectors") {
    pke::Diagnostics d(4,4,4);
    d.RecordFrame({16.7,0.25,0.40,1234.0});
    d.RecordAudio({48000,256,true});
    d.RecordInputOffset(-7.5);
    auto s=d.Summary();
    PKE_REQUIRE(s.frameSamples==1);
    PKE_REQUIRE(s.audioSamples==1);
    PKE_REQUIRE(s.inputSamples==1);
    PKE_REQUIRE_NEAR(s.latestFrame.renderMs,0.40,1e-9);
    PKE_REQUIRE(s.latestAudio.songFrame==48000);
    PKE_REQUIRE(s.latestAudio.underrun);
    PKE_REQUIRE_NEAR(s.latestInputOffsetMs,-7.5,1e-9);
}
