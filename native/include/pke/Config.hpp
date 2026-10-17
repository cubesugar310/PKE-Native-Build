#pragma once
#include "pke/SdlCompat.hpp"
#include <array>
#include <filesystem>

namespace pke {
struct Config {
    std::filesystem::path songsRoot;
    std::filesystem::path charactersRoot;
    std::array<SDL_Scancode, 4> playerKeys{SDL_SCANCODE_LEFT, SDL_SCANCODE_DOWN, SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT};
    bool vsync{false};
    int fpsCap{240};
    double inputOffsetMs{0.0};
    double audioLatencyCompensationMs{0.0};
};

Config LoadConfig(const std::filesystem::path& executableDir,
                  const std::filesystem::path& configPath);
}
