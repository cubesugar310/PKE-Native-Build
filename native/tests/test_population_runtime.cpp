#include "TestHarness.hpp"
#include "pke/PopulationRuntime.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {
using namespace std::chrono_literals;
constexpr std::int64_t kStart=1791989999000;
// The real imported VM takes seconds to initialize. Gate only that slow
// boundary so scheduler ownership, coalescing, cancellation and failure paths
// can be exercised deterministically, without relying on OS scheduling luck.
struct BackendState {
    std::mutex mutex;
    std::condition_variable cv;
    bool initializeEntered=false,releaseInitialize=true,interrupted=false;
    bool failStep=false,failSerialize=false,invalidCounter=false,invalidStepClock=false;
    std::thread::id constructed,initialized,destroyed;
    std::vector<std::int64_t> steps;
};
pke::PopulationSummary SummaryAt(std::int64_t now){
    pke::PopulationSummary value;
    value.registered=15000;value.cpuPlayers=14999;value.nowMs=now;
    value.lastTickMs=kStart+((now-kStart)/5000)*5000;
    value.online=5938;value.live=2000;value.liveCap=2000;value.waiting=919;
    value.localPlayerState="YOU";
    return value;
}
class GatedBackend final:public pke::PopulationBackend {
public:
    explicit GatedBackend(std::shared_ptr<BackendState> state):state_(std::move(state)){
        std::lock_guard lock(state_->mutex);state_->constructed=std::this_thread::get_id();
    }
    ~GatedBackend() override{
        std::lock_guard lock(state_->mutex);state_->destroyed=std::this_thread::get_id();
    }
    pke::PopulationSummary Initialize() override{
        std::unique_lock lock(state_->mutex);
        state_->initialized=std::this_thread::get_id();state_->initializeEntered=true;state_->cv.notify_all();
        state_->cv.wait(lock,[&]{return state_->releaseInitialize||state_->interrupted;});
        if(state_->interrupted)throw std::runtime_error("initialization interrupted");
        auto value=SummaryAt(kStart);if(state_->invalidCounter)value.registered=14999;return value;
    }
    pke::PopulationSummary AdvanceTo(std::int64_t now) override{
        std::lock_guard lock(state_->mutex);state_->steps.push_back(now);
        if(state_->failStep)throw std::runtime_error("sentinel-generation-failed");
        now_=now;auto value=SummaryAt(now);if(state_->invalidStepClock)value.lastTickMs-=10000;return value;
    }
    std::string Serialize() override{
        std::lock_guard lock(state_->mutex);
        if(state_->failSerialize)throw std::runtime_error("sentinel-checkpoint-failed");
        return "{\"now\":"+std::to_string(now_)+"}";
    }
    void RequestInterrupt() noexcept override{
        std::lock_guard lock(state_->mutex);state_->interrupted=true;state_->cv.notify_all();
    }
private:
    std::shared_ptr<BackendState> state_;
    std::int64_t now_{kStart};
};
auto Factory(const std::shared_ptr<BackendState>& state){
    return [state]{return std::make_unique<GatedBackend>(state);};
}
pke::PopulationCheckpoint Checkpoint(pke::PopulationRuntime& runtime){
    auto future=runtime.RequestCheckpoint();
    PKE_REQUIRE(future.wait_for(5s)==std::future_status::ready);
    return future.get();
}
void AwaitInitialization(const std::shared_ptr<BackendState>& state){
    std::unique_lock lock(state->mutex);
    PKE_REQUIRE(state->cv.wait_for(lock,5s,[&]{return state->initializeEntered;}));
}
}

PKE_TEST("Population worker owns backend construction calls and destruction") {
    const auto caller=std::this_thread::get_id();
    auto state=std::make_shared<BackendState>();
    pke::PopulationRuntime runtime(Factory(state));
    PKE_REQUIRE(Checkpoint(runtime).ok);
    const auto snapshot=runtime.Snapshot();
    PKE_REQUIRE(snapshot&&snapshot->registered==15000&&snapshot->cpuPlayers==14999);
    runtime.Stop();runtime.Stop();
    std::lock_guard lock(state->mutex);
    PKE_REQUIRE(state->constructed!=caller);
    PKE_REQUIRE(state->constructed==state->initialized);
    PKE_REQUIRE(state->destroyed==state->initialized);
}

PKE_TEST("Population worker coalesces render submissions while initialization is busy") {
    auto state=std::make_shared<BackendState>();state->releaseInitialize=false;
    pke::PopulationRuntime runtime(Factory(state));AwaitInitialization(state);
    const auto started=std::chrono::steady_clock::now();
    for(int frame=0;frame<100000;frame++)
        PKE_REQUIRE(runtime.AdvanceTo(kStart+frame*10));
    PKE_REQUIRE(std::chrono::steady_clock::now()-started<1s);
    PKE_REQUIRE(!runtime.Snapshot());
    auto checkpoint=runtime.RequestCheckpoint();
    {
        std::lock_guard lock(state->mutex);state->releaseInitialize=true;state->cv.notify_all();
    }
    PKE_REQUIRE(checkpoint.wait_for(5s)==std::future_status::ready);
    const auto result=checkpoint.get();PKE_REQUIRE(result.ok);
    PKE_REQUIRE(result.nowMs==kStart+999990);
    PKE_REQUIRE(runtime.Snapshot()->nowMs==kStart+999990);
    std::lock_guard lock(state->mutex);
    PKE_REQUIRE(state->steps.size()==1);
    PKE_REQUIRE(state->steps.front()==kStart+999990);
    PKE_REQUIRE(state->steps.back()==kStart+999990);
}

PKE_TEST("Population worker advances Home Gameplay and Result for exactly 270 seconds") {
    auto state=std::make_shared<BackendState>();pke::PopulationRuntime runtime(Factory(state));
    PKE_REQUIRE(Checkpoint(runtime).ok);
    std::shared_ptr<const pke::PopulationSummary> home;
    for(int tick=1;tick<=54;tick++){
        PKE_REQUIRE(runtime.AdvanceTo(kStart+tick*5000));PKE_REQUIRE(Checkpoint(runtime).ok);
        if(tick==12)home=runtime.Snapshot();
    }
    PKE_REQUIRE(home&&home->nowMs==kStart+60000);
    PKE_REQUIRE(runtime.Snapshot()->nowMs==kStart+270000);
    PKE_REQUIRE(home->nowMs==kStart+60000); // published summaries stay immutable
    PKE_REQUIRE(runtime.Status()->completedSteps==54);
    PKE_REQUIRE(runtime.Status()->lastStepMs>=0&&runtime.Status()->maxStepMs>=0);
    std::lock_guard lock(state->mutex);
    PKE_REQUIRE(state->steps.size()==54);
    for(std::size_t i=0;i<state->steps.size();i++)
        PKE_REQUIRE(state->steps[i]==kStart+static_cast<std::int64_t>(i+1)*5000);
}

PKE_TEST("Population worker ignores backwards time and rejects invalid Native timestamps") {
    auto state=std::make_shared<BackendState>();pke::PopulationRuntime runtime(Factory(state));
    PKE_REQUIRE(runtime.AdvanceTo(kStart+7001));PKE_REQUIRE(Checkpoint(runtime).ok);
    PKE_REQUIRE(runtime.AdvanceTo(kStart-10000));PKE_REQUIRE(Checkpoint(runtime).ok);
    PKE_REQUIRE(!runtime.AdvanceTo(-1));
    PKE_REQUIRE(!runtime.AdvanceTo(9007199254740992LL));
    const auto snapshot=runtime.Snapshot();
    PKE_REQUIRE(snapshot->nowMs==kStart+7001&&snapshot->lastTickMs==kStart+5000);
    std::lock_guard lock(state->mutex);PKE_REQUIRE(state->steps.size()==1);
}

PKE_TEST("Population worker keeps last good summary and refuses a failed generation checkpoint") {
    auto state=std::make_shared<BackendState>();pke::PopulationRuntime runtime(Factory(state));
    PKE_REQUIRE(Checkpoint(runtime).ok);const auto before=runtime.Snapshot();
    {std::lock_guard lock(state->mutex);state->failStep=true;}
    PKE_REQUIRE(runtime.AdvanceTo(kStart+5000));
    const auto failed=Checkpoint(runtime);PKE_REQUIRE(!failed.ok);
    PKE_REQUIRE(failed.error.find("sentinel-generation-failed")!=std::string::npos);
    PKE_REQUIRE(runtime.Snapshot()==before);
    PKE_REQUIRE(runtime.Status()->phase==pke::PopulationPhase::Failed);
    PKE_REQUIRE(!runtime.AdvanceTo(kStart+10000));
    PKE_REQUIRE(!Checkpoint(runtime).ok);
}

PKE_TEST("Population worker surfaces checkpoint failure without publishing corrupted data") {
    auto state=std::make_shared<BackendState>();pke::PopulationRuntime runtime(Factory(state));
    PKE_REQUIRE(Checkpoint(runtime).ok);const auto before=runtime.Snapshot();
    {std::lock_guard lock(state->mutex);state->failSerialize=true;}
    const auto failed=Checkpoint(runtime);PKE_REQUIRE(!failed.ok);
    PKE_REQUIRE(failed.json.empty());
    PKE_REQUIRE(failed.error.find("sentinel-checkpoint-failed")!=std::string::npos);
    PKE_REQUIRE(runtime.Snapshot()==before);
    PKE_REQUIRE(runtime.Status()->phase==pke::PopulationPhase::Failed);
}

PKE_TEST("Population worker shutdown interrupts startup and resolves waiting checkpoints") {
    auto state=std::make_shared<BackendState>();state->releaseInitialize=false;
    pke::PopulationRuntime runtime(Factory(state));AwaitInitialization(state);
    auto future=runtime.RequestCheckpoint();runtime.Stop();
    PKE_REQUIRE(future.wait_for(1s)==std::future_status::ready);
    PKE_REQUIRE(!future.get().ok);
    PKE_REQUIRE(runtime.Status()->phase==pke::PopulationPhase::Stopped);
    PKE_REQUIRE(!runtime.AdvanceTo(kStart+5000));
    PKE_REQUIRE(!Checkpoint(runtime).ok);
    std::lock_guard lock(state->mutex);PKE_REQUIRE(state->interrupted);
    PKE_REQUIRE(state->constructed==state->destroyed);
}

PKE_TEST("Population worker captures backend construction failures as Native status") {
    pke::PopulationRuntime runtime([]()->std::unique_ptr<pke::PopulationBackend>{
        throw std::runtime_error("sentinel-no-runtime");
    });
    const auto result=Checkpoint(runtime);PKE_REQUIRE(!result.ok);
    PKE_REQUIRE(result.error.find("sentinel-no-runtime")!=std::string::npos);
    PKE_REQUIRE(!runtime.Snapshot());
    PKE_REQUIRE(runtime.Status()->phase==pke::PopulationPhase::Failed);
}

PKE_TEST("Population worker rejects malformed backend counters and stale simulation clocks") {
    for(const bool initialFailure:{true,false}){
        auto state=std::make_shared<BackendState>();state->invalidCounter=initialFailure;
        pke::PopulationRuntime runtime(Factory(state));
        if(!initialFailure){
            PKE_REQUIRE(Checkpoint(runtime).ok);
            {std::lock_guard lock(state->mutex);state->invalidStepClock=true;}
            PKE_REQUIRE(runtime.AdvanceTo(kStart+5000));
        }
        const auto result=Checkpoint(runtime);PKE_REQUIRE(!result.ok);
        PKE_REQUIRE(runtime.Status()->phase==pke::PopulationPhase::Failed);
        if(initialFailure)PKE_REQUIRE(!runtime.Snapshot());
        else PKE_REQUIRE(runtime.Snapshot()->nowMs==kStart);
    }
}

PKE_TEST("Population worker wakes reliably for repeated idle checkpoint requests") {
    auto state=std::make_shared<BackendState>();pke::PopulationRuntime runtime(Factory(state));
    for(int request=0;request<1000;request++){
        const auto checkpoint=Checkpoint(runtime);PKE_REQUIRE(checkpoint.ok);
        PKE_REQUIRE(checkpoint.nowMs==kStart);
    }
    PKE_REQUIRE(runtime.Status()->completedSteps==0);
}

PKE_TEST("Population worker checkpoints an exact partial target before the next normal tick") {
    auto state=std::make_shared<BackendState>();pke::PopulationRuntime runtime(Factory(state));
    PKE_REQUIRE(Checkpoint(runtime).ok);
    PKE_REQUIRE(runtime.AdvanceTo(kStart+1234));
    const auto checkpoint=Checkpoint(runtime);
    PKE_REQUIRE(checkpoint.ok&&checkpoint.nowMs==kStart+1234);
    PKE_REQUIRE(runtime.Snapshot()->nowMs==kStart+1234);
    PKE_REQUIRE(runtime.Snapshot()->lastTickMs==kStart);
    PKE_REQUIRE(runtime.AdvanceTo(kStart+5000));
    PKE_REQUIRE(Checkpoint(runtime).nowMs==kStart+5000);
    PKE_REQUIRE(runtime.Snapshot()->lastTickMs==kStart+5000);
}
