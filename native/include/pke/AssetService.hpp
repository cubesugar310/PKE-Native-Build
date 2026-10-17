#pragma once
#include "pke/Character.hpp"
#include "pke/Chart.hpp"
#include "pke/Config.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace pke {
struct AssetIssue {std::filesystem::path path;std::string message;};
struct SongChartAsset {
    Chart chart;
    std::filesystem::path path;
    std::string difficulty;
    int playerStars{},opponentStars{},playerNoteCount{},opponentNoteCount{};
    double durationMs{},baseBpm{};
    std::optional<double> scrollSpeed;
    std::string lightweightJson,difficultyJson;
};
struct SongAsset {
    std::string folderName,displayName,opponentCharacterId;
    std::filesystem::path folder,instPath,iconPath;
    std::optional<std::filesystem::path> voicesPath;
    // Original candidate order; front() is the reference default chart.
    std::vector<SongChartAsset> charts;
};
struct CharacterAsset {
    std::string id,displayName;
    std::filesystem::path jsonPath;
    std::shared_ptr<const CharacterDefinition> definition;
    std::string sourceSkinId;
};
struct AssetCatalog {
    std::uint64_t generation{};
    std::vector<SongAsset> songs;
    std::vector<CharacterAsset> characters;
    std::vector<AssetIssue> issues;
};
const CharacterAsset* FindCharacterAsset(const AssetCatalog&,std::string_view);

// Owner-thread service. Refresh is an explicit cold-path scan; all later
// queries use immutable catalog data and cached, normalized search metadata.
class AssetService {
public:
    AssetService(const Config&,std::filesystem::path executableDirectory,
                 std::filesystem::path compatibilityDirectory,std::string locale={});
    ~AssetService();
    AssetService(const AssetService&)=delete;
    AssetService& operator=(const AssetService&)=delete;
    std::shared_ptr<const AssetCatalog> Refresh();
    std::shared_ptr<const AssetCatalog> Catalog() const noexcept;
    std::vector<std::size_t> FilterSongs(std::string_view query,Side side,int stars=0);
    std::string PopulationCatalogJson() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
