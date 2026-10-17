#include "TestHarness.hpp"
#include "pke/Config.hpp"
#include <filesystem>

PKE_TEST("LoadConfig defaults beside executable") {
    const auto root = std::filesystem::temp_directory_path() / "pke-native-config-defaults";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto cfg = pke::LoadConfig(root, root / "missing-pke-native.json");
    PKE_REQUIRE(cfg.songsRoot == root / "Songs");
    PKE_REQUIRE(cfg.charactersRoot == root / "Characters");
    PKE_REQUIRE(cfg.playerKeys[0] == SDL_SCANCODE_LEFT);
    PKE_REQUIRE(cfg.playerKeys[1] == SDL_SCANCODE_DOWN);
    PKE_REQUIRE(cfg.playerKeys[2] == SDL_SCANCODE_UP);
    PKE_REQUIRE(cfg.playerKeys[3] == SDL_SCANCODE_RIGHT);
    PKE_REQUIRE(!cfg.vsync);
    PKE_REQUIRE(cfg.fpsCap == 240);
    PKE_REQUIRE_NEAR(cfg.inputOffsetMs, 0.0, 1e-9);
    PKE_REQUIRE_NEAR(cfg.audioLatencyCompensationMs, 0.0, 1e-9);
}
