#pragma once
#include <cstdint>
#include <array>
#include <map>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pke {

struct PlayerProgressMarker {
    std::uint64_t matches{0};
    std::uint64_t recentAt{0};
    std::uint64_t accuracyMatches{0};
};

struct ImportedPlayerProgress {
    int rp{0};
    int wins{0};
    int losses{0};
    int draws{0};
    std::uint64_t matches{0};
    double accuracy{0.0};
    double accuracySum{0.0};
    std::uint64_t accuracyMatches{0};
    std::uint64_t recentAt{0};
    int peakRp{0};
    std::string onlineSongRangeTier{"beginner"};
    std::string lastOpponentId;
};

struct HalloweenHistory {
    std::uint64_t candyEarned{},onlineCompleted{},onlineWins{},halloweenBossWins{};
    std::vector<std::string> purchases;
};
struct HalloweenSkinSlot {std::string skinId;int price{3000};};
struct HalloweenDevConfig {
    std::array<HalloweenSkinSlot,4> skinSlots;
    std::array<std::string,3> bossSongRefs;
    std::string eventOverride{"auto"};
};
struct HalloweenData {
    std::uint64_t halloweenCandy{};
    std::vector<std::string> purchasedSkinIds,unlockedTitleIds,claimedRewardIds;
    std::map<std::string,HalloweenHistory> halloweenHistory;
    HalloweenDevConfig halloweenDev;
};
struct LegacyFullSaveData : HalloweenData {
    bool valid{false};
    std::filesystem::path sourcePath;
    std::string exportedAt;
    ImportedPlayerProgress progress;
    std::string playerName;
    std::string equippedTitle;
    std::string selectedSkin;
    double masterVolume{1.0};
    std::string scrollDirection{"up"};
    std::string judgementPosition{"center"};
    std::string characterLayoutsJson{"{}"};
    std::string characterIdentityHistoryJson{"{}"};
    std::vector<std::string> earnedTitleIds;
};

struct NativeSaveSummary : HalloweenData {
    bool valid{false};
    int version{1};
    std::string importedAt;
    std::string legacyExportedAt;
    std::string legacyBackupFile{"legacy-full-save-v1.json"};
    std::string legacySourcePath;
    ImportedPlayerProgress progress;
    std::string playerName;
    std::string equippedTitle;
    std::string selectedSkin;
    double masterVolume{1.0};
    std::string scrollDirection{"up"};
    std::string judgementPosition{"center"};
    std::string characterLayoutsJson{"{}"};
    std::string characterIdentityHistoryJson{"{}"};
    std::vector<std::string> earnedTitleIds;
};

enum class LegacyImportStatus {
    Imported,
    NativeIsNewerOrEqual,
    NoLegacySave,
    InvalidLegacySave,
    WriteFailed
};

struct LegacyImportResult {
    LegacyImportStatus status{LegacyImportStatus::NoLegacySave};
    std::filesystem::path sourcePath;
    std::filesystem::path nativeSavePath;
    std::string message;
};

LegacyFullSaveData ReadLegacyFullSave(const std::filesystem::path& path);
NativeSaveSummary ReadNativeSaveSummary(const std::filesystem::path& path);
NativeSaveSummary ParseNativeSaveSummary(const std::string& json);
std::string SerializeNativeSaveSummary(const NativeSaveSummary& summary);
void WriteNativeSaveSummary(const std::filesystem::path& path, const NativeSaveSummary& summary);

int CompareProgressMarkers(const PlayerProgressMarker& left, const PlayerProgressMarker& right);
PlayerProgressMarker ProgressMarkerFor(const ImportedPlayerProgress& progress);

std::vector<std::filesystem::path> DiscoverLegacyFullSaves(
    const std::filesystem::path& executableDir,
    const std::filesystem::path& localAppData,
    const std::optional<std::filesystem::path>& explicitPath = std::nullopt);

std::filesystem::path DefaultNativeSaveRoot(
    const std::filesystem::path& executableDir,
    const std::filesystem::path& localAppData);

LegacyImportResult ImportLegacyFullSave(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& nativeSaveRoot);

LegacyImportResult AutoImportLegacyFullSaves(
    const std::filesystem::path& executableDir,
    const std::filesystem::path& localAppData,
    const std::optional<std::filesystem::path>& explicitPath = std::nullopt);

} // namespace pke
