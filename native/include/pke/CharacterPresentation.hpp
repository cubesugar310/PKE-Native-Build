#pragma once
#include "pke/AssetService.hpp"
#include "pke/CharacterAnimator.hpp"
#include <map>
#include <optional>
#include <utility>

namespace pke {
enum class CharacterRole {Player,Opponent};
struct CharacterTransform {
    double scale{1},offsetX{},offsetY{};
    std::optional<bool> flipXOverride;
    std::map<std::string,CharacterPoint,std::less<>> poses;
};
void PrepareCharacterGeometry(CharacterDefinition&);
FramePlacement ComputePracticeCharacterPlacement(const CharacterDefinition&,AnimationFrameView,CharacterRole,bool opponentPerspective,const CharacterTransform&);
std::pair<CharacterTransform,CharacterTransform> LoadCharacterTransforms(const std::filesystem::path& compatibilityDirectory,std::string_view savedLayouts,const CharacterAsset* player,const CharacterAsset* opponent,std::string_view identityHistory="{}");
}
