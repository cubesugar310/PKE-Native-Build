#pragma once
#include "pke/SdlCompat.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace pke {
struct CharacterPoint {double x{},y{};};
struct AnimationAnchor {double width{1},height{1};};
struct AtlasFrame {
    std::string name;
    SDL_Rect source{};
    double frameX{0},frameY{0},frameWidth{0},frameHeight{0};
    bool rotated{false};
};
struct AnimationDef {
    std::string anim;
    std::string prefix;
    std::vector<int> indices;
    double fps{24.0};
    bool loop{false};
    CharacterPoint offset{};
    std::vector<size_t> frameIndices;
    AnimationAnchor anchor;
};
struct CharacterDefinition {
    std::filesystem::path imagePath;
    std::unordered_map<std::string,AnimationDef> animations;
    std::vector<std::string> animationOrder;
    double scale{1.0};
    SDL_Point position{};
    bool flipX{false};
    std::optional<bool> opponentFlipX;
    bool noAntialiasing{false};
    double singDuration{4.0};
    std::vector<AtlasFrame> atlas;
    std::vector<CharacterPoint> playerTrimOffsets;
    std::vector<std::string> warnings;
    AnimationAnchor idleAnchor;
    bool geometryPrepared{};
};
struct FramePlacement {
    SDL_Rect source{};
    double destinationX{}; double destinationY{}; double destinationW{}; double destinationH{};
    bool rotated{false}; bool flipX{false};
};
}
