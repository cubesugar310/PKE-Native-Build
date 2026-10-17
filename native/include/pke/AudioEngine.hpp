#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pke {
class Diagnostics;
enum class AudioState { Stopped, Ready, Playing, Paused, DeviceLost, Finished, Error };
class IAudioClock {
public:
    virtual ~IAudioClock() = default;
    virtual bool Start() = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual void Stop() = 0;
    virtual double SongTimeMs() const = 0;
    virtual AudioState State() const = 0;
    virtual std::string LastErrorCopy() const = 0;
};
struct DecodedAudio {
    uint32_t sampleRate{0};
    uint32_t channels{0};
    std::vector<float> pcm;
    uint64_t frameCount{0};
};
class AudioEngine : public IAudioClock {
public:
    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&)=delete;
    AudioEngine& operator=(const AudioEngine&)=delete;

    bool Load(const std::filesystem::path& instPath,const std::optional<std::filesystem::path>& voicesPath=std::nullopt);
    void LoadDecodedForTest(DecodedAudio inst,std::optional<DecodedAudio> voices);
    bool Start() override;
    void Pause() override;
    void Resume() override;
    void Stop() override;
    void SeekFrames(uint64_t frame);
    uint64_t SongFrame() const { return songFrame_.load(std::memory_order_relaxed); }
    double SongTimeMs() const override;
    AudioState State() const override { return state_.load(std::memory_order_relaxed); }
    const std::string& LastError() const { return lastError_; }
    std::string LastErrorCopy() const override;
    void SetLatencyCompensationMs(double ms){ latencyCompensationMs_=ms; }
    uint32_t OutputSampleRate() const { return outputSampleRate_; }
    uint32_t OutputChannels() const { return outputChannels_; }
    uint64_t DebugInstFrame() const;
    uint64_t DebugVoicesFrame() const;
    void NotifyDeviceLost(std::string message);
    void NotifyDeviceLostFromAudioThread() noexcept;
    void SetDiagnostics(Diagnostics* diagnostics) noexcept { diagnostics_=diagnostics; }
    void MixFrames(float* output,uint32_t frameCount) noexcept;
private:
    struct DeviceImpl;
    std::unique_ptr<DeviceImpl> device_;
    static DecodedAudio ConvertTo(const DecodedAudio& src,uint32_t sampleRate,uint32_t channels);
    void SetError(std::string message,AudioState state=AudioState::Error);
    bool EnsureDevice();

    DecodedAudio inst_;
    std::optional<DecodedAudio> voices_;
    uint32_t outputSampleRate_{0};
    uint32_t outputChannels_{0};
    std::atomic<uint64_t> songFrame_{0};
    std::atomic<AudioState> state_{AudioState::Stopped};
    double latencyCompensationMs_{0.0};
    std::string lastError_;
    Diagnostics* diagnostics_{nullptr};
};
}
