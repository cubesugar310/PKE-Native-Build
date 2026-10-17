#include "pke/AudioEngine.hpp"
#include "pke/Diagnostics.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

#ifdef PKE_NATIVE_WITH_AUDIO_DEPS
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <stb_vorbis.c>
#endif

namespace pke {
struct AudioEngine::DeviceImpl {
#ifdef PKE_NATIVE_WITH_AUDIO_DEPS
    ma_device device{};
    bool initialized{false};
#endif
};

AudioEngine::AudioEngine(IAudioOutput* output):device_(std::make_unique<DeviceImpl>()),output_(output){}
AudioEngine::~AudioEngine(){
    if(output_) output_->Stop();
#ifdef PKE_NATIVE_WITH_AUDIO_DEPS
    if(device_ && device_->initialized){ ma_device_uninit(&device_->device); device_->initialized=false; }
#endif
}

DecodedAudio AudioEngine::ConvertTo(const DecodedAudio& src,uint32_t sr,uint32_t ch){
    if(src.sampleRate==0||src.channels==0||src.frameCount==0) return {sr,ch,{},0};
    const uint64_t dstFrames=static_cast<uint64_t>(std::llround(double(src.frameCount)*double(sr)/double(src.sampleRate)));
    DecodedAudio out; out.sampleRate=sr; out.channels=ch; out.frameCount=dstFrames; out.pcm.resize(dstFrames*ch);
    for(uint64_t i=0;i<dstFrames;++i){
        const double pos=double(i)*double(src.sampleRate)/double(sr); const uint64_t i0=std::min<uint64_t>(static_cast<uint64_t>(pos),src.frameCount-1); const uint64_t i1=std::min<uint64_t>(i0+1,src.frameCount-1); const float frac=float(pos-double(i0));
        for(uint32_t c=0;c<ch;++c){
            auto sampleAt=[&](uint64_t frame)->float{ if(src.channels==1) return src.pcm[frame]; if(ch==1){ float sum=0; for(uint32_t sc=0;sc<src.channels;++sc)sum+=src.pcm[frame*src.channels+sc]; return sum/float(src.channels);} uint32_t sc=std::min(c,src.channels-1); return src.pcm[frame*src.channels+sc]; };
            out.pcm[i*ch+c]=sampleAt(i0)+(sampleAt(i1)-sampleAt(i0))*frac;
        }
    }
    return out;
}
void AudioEngine::SetError(std::string m,AudioState st){ lastError_=std::move(m); state_.store(st,std::memory_order_relaxed); }
void AudioEngine::LoadDecodedForTest(DecodedAudio inst,std::optional<DecodedAudio> voices){
    if(inst.sampleRate==0||inst.channels==0||inst.frameCount==0||inst.pcm.size()<inst.frameCount*inst.channels) throw std::runtime_error("Invalid Inst PCM");
    outputSampleRate_=inst.sampleRate; outputChannels_=inst.channels; inst_=ConvertTo(inst,outputSampleRate_,outputChannels_); voices_.reset(); if(voices) voices_=ConvertTo(*voices,outputSampleRate_,outputChannels_); songFrame_=0; lastError_.clear(); state_=AudioState::Ready;
}
#ifdef PKE_NATIVE_WITH_AUDIO_DEPS
static DecodedAudio DecodeVorbisFile(const std::filesystem::path& p){
    int ch=0,sr=0; short* raw=nullptr; int frames=stb_vorbis_decode_filename(p.string().c_str(),&ch,&sr,&raw); if(frames<0||!raw) throw std::runtime_error("Vorbis decode failed: "+p.string());
    DecodedAudio a; a.sampleRate=uint32_t(sr); a.channels=uint32_t(ch); a.frameCount=uint64_t(frames); a.pcm.resize(a.frameCount*a.channels); for(size_t i=0;i<a.pcm.size();++i)a.pcm[i]=float(raw[i])/32768.0f; std::free(raw); return a;
}
#endif
bool AudioEngine::Load(const std::filesystem::path& instPath,const std::optional<std::filesystem::path>& voicesPath){
    try {
#ifdef PKE_NATIVE_WITH_AUDIO_DEPS
        auto inst=DecodeVorbisFile(instPath); std::optional<DecodedAudio> voices; if(voicesPath) voices=DecodeVorbisFile(*voicesPath); LoadDecodedForTest(std::move(inst),std::move(voices)); return true;
#else
        (void)instPath; (void)voicesPath; SetError("OGG decode unavailable in offline-core build"); return false;
#endif
    } catch(const std::exception& e){ SetError(e.what()); return false; }
}
#ifdef PKE_NATIVE_WITH_AUDIO_DEPS
static void maDataCallback(ma_device* d,void* output,const void*,ma_uint32 frames){ auto* self=static_cast<AudioEngine*>(d->pUserData); self->MixFrames(static_cast<float*>(output),frames); }
static void maNotificationCallback(const ma_device_notification* n){
    if(!n||!n->pDevice) return;
    auto* self=static_cast<AudioEngine*>(n->pDevice->pUserData);
    if(self && n->type==ma_device_notification_type_stopped) self->NotifyDeviceLostFromAudioThread();
}
#endif
bool AudioEngine::EnsureDevice(){
#ifdef PKE_NATIVE_WITH_AUDIO_DEPS
    if(device_->initialized) return true;
    ma_device_config cfg=ma_device_config_init(ma_device_type_playback); cfg.playback.format=ma_format_f32; cfg.playback.channels=outputChannels_; cfg.sampleRate=outputSampleRate_; cfg.dataCallback=maDataCallback; cfg.notificationCallback=maNotificationCallback; cfg.pUserData=this;
    if(ma_device_init(nullptr,&cfg,&device_->device)!=MA_SUCCESS){ SetError("Failed to initialize WASAPI/miniaudio playback device"); return false; } device_->initialized=true; return true;
#else
    return true;
#endif
}
bool AudioEngine::Start(){
    if(State()!=AudioState::Ready && State()!=AudioState::Paused) return false;
    songFrame_.store(State()==AudioState::Ready?0:SongFrame(),std::memory_order_relaxed);
    state_=AudioState::Playing;
    if(output_){
        std::string error;
        if(!output_->Start(*this,outputSampleRate_,outputChannels_,error)){
            SetError(error.empty() ? "Failed to start audio output" : std::move(error));
            return false;
        }
        return true;
    }
#ifdef PKE_NATIVE_WITH_AUDIO_DEPS
    if(!EnsureDevice()) return false;
    if(ma_device_start(&device_->device)!=MA_SUCCESS){ SetError("Failed to start WASAPI/miniaudio playback device"); return false; }
#endif
    return true;
}
void AudioEngine::Pause(){ if(State()==AudioState::Playing) state_=AudioState::Paused; }
void AudioEngine::Resume(){ if(State()==AudioState::Paused) state_=AudioState::Playing; }
void AudioEngine::Stop(){ state_=AudioState::Stopped;
    if(output_){ output_->Stop(); return; }
#ifdef PKE_NATIVE_WITH_AUDIO_DEPS
    if(device_&&device_->initialized) ma_device_stop(&device_->device);
#endif
}
void AudioEngine::SeekFrames(uint64_t f){ songFrame_.store(std::min<uint64_t>(f,inst_.frameCount),std::memory_order_relaxed); }
double AudioEngine::SongTimeMs() const { if(outputSampleRate_==0) return latencyCompensationMs_; return double(SongFrame())*1000.0/double(outputSampleRate_)+latencyCompensationMs_; }
uint64_t AudioEngine::DebugInstFrame() const { return std::min<uint64_t>(SongFrame(),inst_.frameCount); }
uint64_t AudioEngine::DebugVoicesFrame() const { return voices_?std::min<uint64_t>(SongFrame(),voices_->frameCount):DebugInstFrame(); }
std::string AudioEngine::LastErrorCopy() const {
    if(State()==AudioState::DeviceLost && lastError_.empty()) return "Audio device lost";
    return lastError_;
}
void AudioEngine::NotifyDeviceLost(std::string m){ SetError(std::move(m),AudioState::DeviceLost); }
void AudioEngine::NotifyDeviceLostFromAudioThread() noexcept {
    const auto current=State();
    if(current!=AudioState::Stopped && current!=AudioState::Finished && current!=AudioState::Error) state_.store(AudioState::DeviceLost,std::memory_order_relaxed);
}
void AudioEngine::MixFrames(float* out,uint32_t frames) noexcept {
    if(!out||outputChannels_==0) return;
    const size_t total=size_t(frames)*outputChannels_;
    std::fill_n(out,total,0.0f);
    if(State()!=AudioState::Playing) return;
    uint64_t cursor=songFrame_.load(std::memory_order_relaxed);
    for(uint32_t f=0;f<frames;++f){ if(cursor>=inst_.frameCount){ state_.store(AudioState::Finished,std::memory_order_relaxed); break; }
        for(uint32_t c=0;c<outputChannels_;++c){ float v=inst_.pcm[cursor*outputChannels_+c]; if(voices_&&cursor<voices_->frameCount)v+=voices_->pcm[cursor*outputChannels_+c]; out[size_t(f)*outputChannels_+c]=v; }
        ++cursor;
    }
    songFrame_.store(cursor,std::memory_order_relaxed);
    if(diagnostics_) diagnostics_->RecordAudio({cursor,frames,false});
}
}
