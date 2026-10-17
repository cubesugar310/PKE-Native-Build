#pragma once
#include "pke/AssetLocator.hpp"
#include "pke/Character.hpp"
namespace pke {
CharacterDefinition LoadCharacter(const CharacterAssetPaths& paths);
FramePlacement ComputeFramePlacement(const AtlasFrame& frame, SDL_Point authoredOffset, double scale, bool flipX);
}
