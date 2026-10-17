#pragma once
#include "pke/Config.hpp"
#include <filesystem>
#include <optional>
#include <string_view>

namespace pke {
struct CharacterAssetPaths {
    std::filesystem::path json;
    std::filesystem::path xml;
    std::filesystem::path image;
};

class AssetLocator {
public:
    AssetLocator(const Config& config, std::filesystem::path executableDir);
    std::filesystem::path ResolveSongFolder(std::string_view songId) const;
    std::filesystem::path ResolveChartFile(const std::filesystem::path& songFolder, std::string_view difficulty) const;
    std::filesystem::path ResolveInst(const std::filesystem::path& songFolder) const;
    std::optional<std::filesystem::path> ResolveVoices(const std::filesystem::path& songFolder) const;
    CharacterAssetPaths ResolveCharacter(std::string_view id) const;
private:
    std::filesystem::path songsRoot_;
    std::filesystem::path charactersRoot_;
};
}
