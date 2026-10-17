#include "TestHarness.hpp"
#include "pke/AudioEngine.hpp"
#include "pke/Diagnostics.hpp"
#include <vector>
#include <atomic>
#include <cstdlib>
#include <new>

namespace { std::atomic<bool> g_trackAlloc{false}; std::atomic<size_t> g_allocCount{0}; }
void* operator new(std::size_t n){ if(g_trackAlloc.load(std::memory_order_relaxed)) g_allocCount.fetch_add(1,std::memory_order_relaxed); if(void* p=std::malloc(n)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n){ if(g_trackAlloc.load(std::memory_order_relaxed)) g_allocCount.fetch_add(1,std::memory_order_relaxed); if(void* p=std::malloc(n)) return p; throw std::bad_alloc(); }
void* operator new(std::size_t n,const std::nothrow_t&) noexcept { if(g_trackAlloc.load(std::memory_order_relaxed)) g_allocCount.fetch_add(1,std::memory_order_relaxed); return std::malloc(n); }
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept { if(g_trackAlloc.load(std::memory_order_relaxed)) g_allocCount.fetch_add(1,std::memory_order_relaxed); return std::malloc(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
void operator delete(void* p,const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p,const std::nothrow_t&) noexcept { std::free(p); }

namespace { pke::DecodedAudio pcm(uint32_t sr,uint32_t ch,uint64_t frames,float v=0.1f){ return {sr,ch,std::vector<float>(frames*ch,v),frames}; } }
PKE_TEST("Audio song clock advances only by mixed frames and pause freezes it") {
    pke::AudioEngine a; a.LoadDecodedForTest(pcm(1000,1,10000),std::nullopt); a.SetLatencyCompensationMs(7.5); a.Start(); std::vector<float> out(100); a.MixFrames(out.data(),100); PKE_REQUIRE(a.SongFrame()==100); PKE_REQUIRE_NEAR(a.SongTimeMs(),107.5,1e-9); a.Pause(); a.MixFrames(out.data(),100); PKE_REQUIRE(a.SongFrame()==100); a.Resume(); a.MixFrames(out.data(),50); PKE_REQUIRE(a.SongFrame()==150);
}
PKE_TEST("Audio shared cursor never drifts during long synthetic playback") {
    pke::AudioEngine a; a.LoadDecodedForTest(pcm(48000,2,48000*5,0.1f),pcm(48000,2,48000*5,0.2f)); a.Start(); std::vector<float> out(512*2); for(int i=0;i<400;++i)a.MixFrames(out.data(),512); PKE_REQUIRE(a.SongFrame()==uint64_t(400*512)); PKE_REQUIRE(a.DebugInstFrame()==a.DebugVoicesFrame());
}
PKE_TEST("Audio device loss is explicit and never uses wall clock fallback") {
    pke::AudioEngine a; a.LoadDecodedForTest(pcm(48000,2,1000),std::nullopt); a.Start(); float out[20]{}; a.MixFrames(out,10); auto before=a.SongFrame(); a.NotifyDeviceLost("test device removed"); PKE_REQUIRE(a.State()==pke::AudioState::DeviceLost); a.MixFrames(out,10); PKE_REQUIRE(a.SongFrame()==before); PKE_REQUIRE(a.LastError().find("test device removed")!=std::string::npos);
}

PKE_TEST("Audio MixFrames performs zero heap allocations after preload") {
    pke::AudioEngine a; a.LoadDecodedForTest(pcm(48000,2,4096),pcm(48000,2,4096)); a.Start(); std::vector<float> out(512*2); g_allocCount=0; g_trackAlloc=true; a.MixFrames(out.data(),512); g_trackAlloc=false; PKE_REQUIRE(g_allocCount.load()==0);
}
PKE_TEST("Audio callback records telemetry and realtime device loss is allocation free") {
    pke::Diagnostics d(4,4,4);
    pke::AudioEngine a; a.SetDiagnostics(&d); a.LoadDecodedForTest(pcm(48000,2,4096),std::nullopt); a.Start();
    float out[512*2]{}; a.MixFrames(out,512);
    auto summary=d.Summary();
    PKE_REQUIRE(summary.audioSamples==1);
    PKE_REQUIRE(summary.latestAudio.songFrame==512);
    PKE_REQUIRE(summary.latestAudio.callbackFrames==512);
    g_allocCount=0; g_trackAlloc=true; a.NotifyDeviceLostFromAudioThread(); g_trackAlloc=false;
    PKE_REQUIRE(g_allocCount.load()==0);
    PKE_REQUIRE(a.State()==pke::AudioState::DeviceLost);
    PKE_REQUIRE(a.LastErrorCopy().find("device")!=std::string::npos);
}
