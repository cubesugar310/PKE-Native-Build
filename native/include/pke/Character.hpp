#pragma once
#include "pke/SdlCompat.hpp"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace pke {
struct AtlasFrame {
    std::string name;
    SDL_Rect source{};
    int frameX{0}; int frameY{0}; int frameWidth{0}; int frameHeight{0};
    bool rotated{false};
};
struct AnimationDef {
    std::string anim;
    std::string prefix;
    std::vector<int> indices;
    double fps{24.0};
    bool loop{false};
    SDL_Point offset{};
    std::vector<size_t> frameIndices;
};
struct CharacterDefinition {
    std::filesystem::path imagePath;
    std::unordered_map<std::string,AnimationDef> animations;
    double scale{1.0};
    SDL_Point position{};
    bool flipX{false};
    bool noAntialiasing{false};
    double singDuration{4.0};
    std::vector<AtlasFrame> atlas;
    std::vector<std::string> warnings;
};
struct FramePlacement {
    SDL_Rect source{};
    double destinationX{}; double destinationY{}; double destinationW{}; double destinationH{};
    bool rotated{false}; bool flipX{false};
};
}
