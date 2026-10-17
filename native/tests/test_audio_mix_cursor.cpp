#include "TestHarness.hpp"
#include "pke/AudioEngine.hpp"
#include "ManualAudioOutput.hpp"
#include <vector>
namespace { pke::DecodedAudio pcm(uint32_t sr,uint32_t ch,uint64_t frames,float v){ pke::DecodedAudio a{sr,ch,std::vector<float>(frames*ch,v),frames}; return a; } }
PKE_TEST("AudioEngine converts to one format and mixes Inst Voices from same cursor") {
    pke::test::ManualAudioOutput output; pke::AudioEngine a(&output); a.LoadDecodedForTest(pcm(48000,2,8,0.25f), pcm(24000,1,4,0.5f)); PKE_REQUIRE(a.State()==pke::AudioState::Ready); PKE_REQUIRE(a.OutputSampleRate()==48000); PKE_REQUIRE(a.OutputChannels()==2); a.Start(); float out[8]{}; a.MixFrames(out,4); PKE_REQUIRE(a.SongFrame()==4); for(float x:out) PKE_REQUIRE_NEAR(x,0.75,1e-5);
}
PKE_TEST("AudioEngine optional Voices remains valid") {
    pke::test::ManualAudioOutput output; pke::AudioEngine a(&output); a.LoadDecodedForTest(pcm(48000,2,4,0.2f),std::nullopt); a.Start(); float out[4]{}; a.MixFrames(out,2); PKE_REQUIRE_NEAR(out[0],0.2,1e-6); PKE_REQUIRE(a.SongFrame()==2);
}
PKE_TEST("Master volume scales both stems without changing the audio clock") {
    pke::test::ManualAudioOutput output;pke::AudioEngine a(&output);
    a.LoadDecodedForTest(pcm(48000,2,8,0.25f),pcm(48000,2,8,0.5f));a.SetMasterVolume(0.4);a.Start();
    float out[4]{};a.MixFrames(out,2);for(float value:out)PKE_REQUIRE_NEAR(value,0.3,1e-6);
    a.SetMasterVolume(0);a.MixFrames(out,2);for(float value:out)PKE_REQUIRE(value==0);PKE_REQUIRE(a.SongFrame()==4);
}
