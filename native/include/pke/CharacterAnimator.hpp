#pragma once
#include "pke/Character.hpp"
#include <cstddef>
#include <string_view>

namespace pke {
struct AnimationFrameView {
    size_t atlasFrameIndex{0};
    std::string_view animationName{};
    double animationStartMs{0.0};
};
class CharacterAnimator {
public:
    void SetCharacter(const CharacterDefinition* character);
    void TriggerSing(int lane,double songTimeMs,double holdUntilMs);
    void ReturnToIdle(double songTimeMs);
    void Update(double songTimeMs);
    AnimationFrameView CurrentFrame() const;
private:
    const AnimationDef* FindAnimation(std::string_view name) const;
    const CharacterDefinition* character_{nullptr};
    const AnimationDef* current_{nullptr};
    std::string_view currentName_{};
    double animationStartMs_{0.0};
    double holdUntilMs_{0.0};
    double currentSongTimeMs_{0.0};
};
}
