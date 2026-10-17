#pragma once
#include "pke/QuickJsRuntime.hpp"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <string>

namespace pke {
// Render-facing data contains no profiles, history, rooms or checkpoint graph.
struct PopulationSummary {
    int registered{},cpuPlayers{},online{},live{},liveCap{},waiting{};
    std::int64_t nowMs{},lastTickMs{};
    std::string localPlayerState;
};
enum class PopulationPhase {Starting,Ready,Failed,Stopped};
struct PopulationStatus {
    PopulationPhase phase{PopulationPhase::Starting};
    std::string error;
    std::uint64_t completedSteps{};
    double initializationMs{},lastStepMs{},maxStepMs{};
};
struct PopulationCheckpoint {
    bool ok{};
    std::string json,error;
    std::int64_t nowMs{};
};

// All methods except the atomic cancellation request belong to the worker.
class PopulationBackend {
public:
    virtual ~PopulationBackend()=default;
    virtual PopulationSummary Initialize()=0;
    virtual PopulationSummary AdvanceTo(std::int64_t targetMs)=0;
    virtual std::string Serialize()=0;
    virtual void RequestInterrupt() noexcept=0;
};

struct PopulationRuntimeOptions {
    std::filesystem::path compatibilityDirectory;
    std::string initializeJson;
    std::shared_future<std::string> initializeJsonFuture;
    std::string checkpointJson;
    std::string locale; // empty selects the Native OS locale
    QuickJsLimits limits{512u*1024u*1024u,256u*1024u,120000};
};

class PopulationRuntime {
public:
    using BackendFactory=std::function<std::unique_ptr<PopulationBackend>()>;
    explicit PopulationRuntime(BackendFactory factory);
    explicit PopulationRuntime(PopulationRuntimeOptions options);
    ~PopulationRuntime();
    PopulationRuntime(const PopulationRuntime&)=delete;
    PopulationRuntime& operator=(const PopulationRuntime&)=delete;

    // Atomic max only: no file access, VM call, or per-frame job allocation.
    // Valid older requests are ignored. Invalid or stopped requests return false.
    bool AdvanceTo(std::int64_t targetMs) noexcept;
    std::shared_ptr<const PopulationSummary> Snapshot() const noexcept;
    std::shared_ptr<const PopulationStatus> Status() const noexcept;
    // Explicit, infrequent full-state transfer. Captures after at least the
    // latest target submitted before this request; later updates cannot starve it.
    std::future<PopulationCheckpoint> RequestCheckpoint();
    // Cancels in-flight VM work and joins. Save the desired final checkpoint
    // before calling Stop. Destruction/Stop belong to the Native owner thread.
    void Stop() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
