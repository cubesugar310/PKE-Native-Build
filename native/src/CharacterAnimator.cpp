#include "pke/CharacterAnimator.hpp"
#include <algorithm>
#include <cmath>

namespace pke {
const AnimationDef* CharacterAnimator::FindAnimation(std::string_view name) const {
    if(!character_) return nullptr;
    auto it=character_->animations.find(std::string(name));
    if(it!=character_->animations.end() && !it->second.frameIndices.empty()) return &it->second;
    auto idle=character_->animations.find("idle");
    return idle!=character_->animations.end() && !idle->second.frameIndices.empty()?&idle->second:nullptr;
}
void CharacterAnimator::SetCharacter(const CharacterDefinition* c){ character_=c; currentSongTimeMs_=0; holdUntilMs_=0; ReturnToIdle(0.0); }
void CharacterAnimator::TriggerSing(int lane,double t,double holdUntil){
    static constexpr const char* names[4]={"singLEFT","singDOWN","singUP","singRIGHT"};
    if(lane<0||lane>3) return;
    current_=FindAnimation(names[lane]);
    currentName_=current_ ? current_->anim : std::string_view{};
    animationStartMs_=t;
    holdUntilMs_=std::max(t,holdUntil);
    currentSongTimeMs_=t;
}
void CharacterAnimator::ReturnToIdle(double t){ current_=FindAnimation("idle"); currentName_=current_?current_->anim:std::string_view{}; animationStartMs_=t; holdUntilMs_=0; if(currentSongTimeMs_<t) currentSongTimeMs_=t; }
void CharacterAnimator::Update(double t){ currentSongTimeMs_=t; if(current_ && currentName_!="idle" && holdUntilMs_>0 && t>=holdUntilMs_) ReturnToIdle(holdUntilMs_); currentSongTimeMs_=t; }
AnimationFrameView CharacterAnimator::CurrentFrame() const {
    if(!current_ || current_->frameIndices.empty()) return {};
    const double elapsed=std::max(0.0,currentSongTimeMs_-animationStartMs_);
    const double fps=current_->fps>0?current_->fps:24.0;
    size_t idx=static_cast<size_t>(std::floor(elapsed*fps/1000.0));
    if(current_->loop) idx%=current_->frameIndices.size(); else idx=std::min(idx,current_->frameIndices.size()-1);
    return {current_->frameIndices[idx],currentName_,animationStartMs_};
}
}
