#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

namespace pke {
struct FrameSample { double intervalMs{0}; double updateMs{0}; double renderMs{0}; double songTimeMs{0}; };
struct AudioSample { uint64_t songFrame{0}; uint32_t callbackFrames{0}; bool underrun{false}; };
struct DiagnosticsSnapshot { std::vector<FrameSample> frames; std::vector<AudioSample> audio; std::vector<double> inputOffsetsMs; };
struct DiagnosticsSummary {
    FrameSample latestFrame{};
    AudioSample latestAudio{};
    double latestInputOffsetMs{0.0};
    size_t frameSamples{0};
    size_t audioSamples{0};
    size_t inputSamples{0};
};

class Diagnostics {
public:
    explicit Diagnostics(size_t frameCapacity=600,size_t audioCapacity=2048,size_t inputCapacity=2048);
    void RecordFrame(const FrameSample& sample) noexcept;
    void RecordAudio(const AudioSample& sample) noexcept;
    void RecordInputOffset(double offsetMs) noexcept;
    DiagnosticsSnapshot Snapshot() const;
    DiagnosticsSummary Summary() const noexcept;
    void Reset() noexcept;
private:
    template<class T>
    struct Ring {
        explicit Ring(size_t cap):data(cap){}
        std::vector<T> data;
        size_t head{0};
        size_t count{0};
        void push(const T& v) noexcept { if(data.empty()) return; data[head]=v; head=(head+1)%data.size(); if(count<data.size()) ++count; }
        void clear() noexcept { head=0; count=0; }
        T latest() const noexcept { if(count==0||data.empty()) return {}; return data[(head+data.size()-1)%data.size()]; }
        std::vector<T> snapshot() const { std::vector<T> out; out.reserve(count); if(data.empty()) return out; const size_t start=(head+data.size()-count)%data.size(); for(size_t i=0;i<count;++i) out.push_back(data[(start+i)%data.size()]); return out; }
    };
    Ring<FrameSample> frames_;
    Ring<AudioSample> audio_;
    Ring<double> inputOffsets_;
    std::atomic<uint64_t> latestAudioFrame_{0};
    std::atomic<uint32_t> latestAudioCallbackFrames_{0};
    std::atomic<bool> latestAudioUnderrun_{false};
    std::atomic<size_t> audioSamples_{0};
};
}
