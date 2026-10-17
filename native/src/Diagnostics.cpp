#include "pke/Diagnostics.hpp"
namespace pke {
Diagnostics::Diagnostics(size_t f,size_t a,size_t i):frames_(f),audio_(a),inputOffsets_(i){}
void Diagnostics::RecordFrame(const FrameSample& s) noexcept { frames_.push(s); }
void Diagnostics::RecordAudio(const AudioSample& s) noexcept {
    audio_.push(s);
    latestAudioFrame_.store(s.songFrame,std::memory_order_relaxed);
    latestAudioCallbackFrames_.store(s.callbackFrames,std::memory_order_relaxed);
    latestAudioUnderrun_.store(s.underrun,std::memory_order_relaxed);
    audioSamples_.fetch_add(1,std::memory_order_relaxed);
}
void Diagnostics::RecordInputOffset(double v) noexcept { inputOffsets_.push(v); }
DiagnosticsSnapshot Diagnostics::Snapshot() const { return {frames_.snapshot(),audio_.snapshot(),inputOffsets_.snapshot()}; }
DiagnosticsSummary Diagnostics::Summary() const noexcept {
    DiagnosticsSummary s;
    s.latestFrame=frames_.latest();
    s.latestAudio={latestAudioFrame_.load(std::memory_order_relaxed),latestAudioCallbackFrames_.load(std::memory_order_relaxed),latestAudioUnderrun_.load(std::memory_order_relaxed)};
    s.latestInputOffsetMs=inputOffsets_.latest();
    s.frameSamples=frames_.count;
    s.audioSamples=audioSamples_.load(std::memory_order_relaxed);
    s.inputSamples=inputOffsets_.count;
    return s;
}
void Diagnostics::Reset() noexcept {
    frames_.clear(); audio_.clear(); inputOffsets_.clear();
    latestAudioFrame_.store(0,std::memory_order_relaxed);
    latestAudioCallbackFrames_.store(0,std::memory_order_relaxed);
    latestAudioUnderrun_.store(false,std::memory_order_relaxed);
    audioSamples_.store(0,std::memory_order_relaxed);
}
}
