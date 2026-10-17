#include "TestHarness.hpp"
#include "pke/PopulationRuntime.hpp"
#include <chrono>
#include <filesystem>
#include <thread>

namespace {
using namespace std::chrono_literals;
constexpr std::int64_t kStart=1791989999000;
pke::PopulationRuntimeOptions Options(){
    pke::PopulationRuntimeOptions options;
    options.compatibilityDirectory=PKE_COMPAT_DIR;
    options.initializeJson=R"({"now":1791989999000,"songs":[],"skins":[]})";
    options.locale="en-US";
    return options;
}
pke::PopulationCheckpoint Checkpoint(pke::PopulationRuntime& runtime){
    auto future=runtime.RequestCheckpoint();
    PKE_REQUIRE(future.wait_for(360s)==std::future_status::ready);
    auto result=future.get();
    if(!result.ok)throw std::runtime_error(result.error);
    return result;
}
void AwaitTime(pke::PopulationRuntime& runtime,std::int64_t target){
    const auto deadline=std::chrono::steady_clock::now()+150s;
    while(std::chrono::steady_clock::now()<deadline){
        const auto snapshot=runtime.Snapshot();
        if(snapshot&&snapshot->nowMs>=target)return;
        if(runtime.Status()->phase==pke::PopulationPhase::Failed)
            throw std::runtime_error(runtime.Status()->error);
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("Population worker did not reach its submitted target");
}
}

PKE_TEST("Real QuickJS worker continues 15000 players through the accepted 270 second flow") {
    pke::PopulationRuntime runtime(Options());
    const auto startup=Checkpoint(runtime);
    PKE_REQUIRE(runtime.Snapshot()->registered==15000);
    PKE_REQUIRE(runtime.Snapshot()->cpuPlayers==14999);
    std::shared_ptr<const pke::PopulationSummary> home;
    // These are genuinely processed normal ticks. A sleep/backlog gap instead
    // follows the original single-drain recovery fixture, not 54 queued jobs.
    for(int tick=1;tick<=54;tick++){
        const auto target=kStart+tick*5000;
        PKE_REQUIRE(runtime.AdvanceTo(target));AwaitTime(runtime,target);
        if(tick==12){Checkpoint(runtime);home=runtime.Snapshot();}
        if(tick==48)Checkpoint(runtime);
    }
    PKE_REQUIRE(home&&home->nowMs==kStart+60000);
    const auto final=Checkpoint(runtime);const auto snapshot=runtime.Snapshot();
    PKE_REQUIRE(final.nowMs-startup.nowMs==270000);
    PKE_REQUIRE(snapshot->nowMs==kStart+270000&&snapshot->lastTickMs==kStart+270000);
    PKE_REQUIRE(snapshot->registered==15000&&snapshot->cpuPlayers==14999);
    PKE_REQUIRE(runtime.Status()->completedSteps==54);
    PKE_REQUIRE(home->nowMs==kStart+60000);
    PKE_REQUIRE(final.json.find("\"now\":1791990269000")!=std::string::npos);
    PKE_REQUIRE(final.json.find("\"lastTick\":1791990269000")!=std::string::npos);
    PKE_REQUIRE(runtime.Status()->maxStepMs>0);
    // Restore is a new application generation, with its original recovery
    // window; it must retain the saved clock and real records at startup.
    auto restoredOptions=Options();restoredOptions.checkpointJson=final.json;
    pke::PopulationRuntime restored(std::move(restoredOptions));
    const auto savedAgain=Checkpoint(restored);
    PKE_REQUIRE(savedAgain.nowMs==final.nowMs);
    PKE_REQUIRE(savedAgain.json==final.json);
    PKE_REQUIRE(restored.Snapshot()->registered==15000);
    PKE_REQUIRE(restored.Snapshot()->nowMs==snapshot->nowMs);
}

PKE_TEST("Real QuickJS worker reports missing scripts without a fabricated population") {
    auto options=Options();options.compatibilityDirectory/="missing-directory";
    pke::PopulationRuntime runtime(std::move(options));
    auto future=runtime.RequestCheckpoint();
    PKE_REQUIRE(future.wait_for(10s)==std::future_status::ready);
    PKE_REQUIRE(!future.get().ok);
    PKE_REQUIRE(!runtime.Snapshot());
    PKE_REQUIRE(runtime.Status()->phase==pke::PopulationPhase::Failed);
}
