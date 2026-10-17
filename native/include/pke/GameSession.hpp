#pragma once
#include "pke/AssetLocator.hpp"
#include "pke/AssetService.hpp"
#include "pke/AudioEngine.hpp"
#include "pke/CharacterAnimator.hpp"
#include "pke/CharacterLoader.hpp"
#include "pke/ChartParser.hpp"
#include "pke/Config.hpp"
#include "pke/Diagnostics.hpp"
#include "pke/Renderer.hpp"
#include "pke/RhythmEngine.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace pke {
enum class SessionState { Loading, Ready, Countdown, Playing, Completed, Failed, DeviceLost };
struct SongSelection { std::string songId; std::string difficulty{"hard"}; };

class GameSession {
public:
    GameSession(Config config,std::filesystem::path executableDir,IAudioClock* injectedClock=nullptr);
    ~GameSession();
    bool Load(const SongSelection& selection);
    bool LoadPractice(const SongAsset&,const AssetCatalog&,Side,std::string_view skin);
    void HandleKeyDown(SDL_Scancode key);
    void HandleKeyUp(SDL_Scancode key);
    void Update();
    void Render(bool present=true);
    SessionState State() const { return state_; }
    const std::string& FailureMessage() const { return failureMessage_; }
    const std::vector<std::string>& Warnings() const { return warnings_; }
    const PlayStats& Stats() const { return rhythm_.Stats(); }
    std::string ExportReplay(const std::filesystem::path& compatibilityDirectory) const;
    void RequestStop();
    void SetRenderer(Renderer* renderer){ renderer_=renderer; }
    void ToggleDiagnostics() noexcept { diagnosticsVisible_=!diagnosticsVisible_; }
    bool DiagnosticsVisible() const noexcept { return diagnosticsVisible_; }
    void RecordFrameTiming(double intervalMs,double updateMs,double renderMs) noexcept;
    DiagnosticsSummary DiagnosticsSummaryCopy() const noexcept { return diagnostics_.Summary(); }
private:
    int LaneForKey(SDL_Scancode key) const;
    CharacterDefinition FallbackCharacter(const std::string& label) const;
    void Fail(std::string message);
    void UpdateOpponentAnimations(double songTimeMs);

    Config config_;
    std::filesystem::path executableDir_;
    AssetLocator locator_;
    Diagnostics diagnostics_;
    AudioEngine ownedAudio_;
    IAudioClock* clock_{nullptr};
    bool injectedClock_{false};
    Renderer* renderer_{nullptr};
    SessionState state_{SessionState::Loading};
    std::string failureMessage_;
    std::vector<std::string> warnings_;
    Chart chart_;
    RhythmEngine rhythm_;
    CharacterDefinition bfDef_;
    CharacterDefinition dadDef_;
    CharacterAnimator bfAnimator_;
    CharacterAnimator dadAnimator_;
    size_t nextOpponentNote_{0};
    double lastUpdateSongTimeMs_{0.0};
    std::string judgementText_;
    double lastJudgementAt_{-1000};
    std::string songName_;
    std::string songFolder_,instName_,voicesName_;
    bool practiceOpponent_{};
    struct ReplayInput {double time;int lane;bool up;};
    std::vector<ReplayInput> replayInputs_;
    bool diagnosticsVisible_{false};
};
}
