#pragma once
#include "pke/AtomicSaveFile.hpp"
#include "pke/LegacySaveImporter.hpp"
#include "pke/PopulationRuntime.hpp"

namespace pke {
enum class SaveLoadStatus {Missing,Loaded,Invalid,ReadFailed};
struct SaveResult {bool ok{};std::string error;};
struct PlayerSaveLoad {
    SaveLoadStatus status{SaveLoadStatus::Missing};
    NativeSaveSummary player;
    std::string error;
};
struct PopulationSaveLoad {
    SaveLoadStatus status{SaveLoadStatus::Missing};
    PopulationCheckpoint checkpoint;
    std::string error;
};
void ValidateNativePlayerSave(std::string_view json);
NativeSaveSummary DecodeNativePlayerSave(std::string_view json);
void ImportHalloweenState(HalloweenData&,std::string_view json);
void ImportHalloweenConfig(HalloweenData&,std::string_view json);
std::string NormalizeCharacterLayoutsJson(std::string_view);
std::int64_t ValidateNativePopulationSave(std::string_view json);

// File operations are explicit startup/checkpoint/shutdown work. This service
// has no calls in note judgement, audio callbacks or per-frame rendering.
class SaveService {
public:
    explicit SaveService(std::filesystem::path root,
        std::shared_ptr<SaveFileOps> files=NativeSaveFileOps());
    PlayerSaveLoad LoadPlayer() const;
    PopulationSaveLoad LoadPopulation() const;
    SaveResult SavePlayer(const NativeSaveSummary& player) const;
    SaveResult SavePopulation(const PopulationCheckpoint& checkpoint) const;
private:
    std::filesystem::path root_;
    std::shared_ptr<SaveFileOps> files_;
};
}
