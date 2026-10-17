#pragma once
#include "pke/AudioEngine.hpp"

namespace pke::test {
// The test owns the callback schedule; the real decoder and mixer stay enabled.
class ManualAudioOutput final : public IAudioOutput {
public:
    bool Start(AudioEngine&,uint32_t,uint32_t,std::string&) override { return true; }
    void Stop() noexcept override {}
};
}
