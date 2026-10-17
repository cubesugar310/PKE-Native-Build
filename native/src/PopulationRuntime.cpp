#include "pke/PopulationRuntime.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace pke {
namespace {
constexpr std::int64_t kMaxJsTime=9007199254740991LL;
using Clock=std::chrono::steady_clock;
double Milliseconds(Clock::time_point started){
    return std::chrono::duration<double,std::milli>(Clock::now()-started).count();
}
void Validate(const PopulationSummary& value){
    if(value.registered!=15000||value.cpuPlayers!=14999||value.nowMs<0||value.nowMs>kMaxJsTime||
       value.lastTickMs<0||value.lastTickMs>value.nowMs||value.nowMs-value.lastTickMs>=5000||
       value.localPlayerState!="YOU")
        throw std::runtime_error("Invalid population summary or clock");
    for(const int count:{value.online,value.live,value.liveCap,value.waiting})
        if(count<0||count>15000)throw std::runtime_error("Invalid population counter");
}
}

struct PopulationRuntime::Impl {
    struct Pending {
        std::int64_t target;
        std::promise<PopulationCheckpoint> promise;
    };
    BackendFactory factory;
    std::atomic<std::int64_t> target{0};
    std::atomic<bool> stopping{false};
    std::atomic<std::uint64_t> revision{0};
    std::atomic<std::shared_ptr<const PopulationSummary>> summary;
    std::atomic<std::shared_ptr<const PopulationStatus>> status{
        std::make_shared<const PopulationStatus>()};
    std::mutex mutex,joinMutex;
    std::deque<Pending> pending;
    PopulationBackend* activeBackend{nullptr}; // borrowed only under mutex
    std::thread worker;

    explicit Impl(BackendFactory value):factory(std::move(value)){
        worker=std::thread([this]{Run();});
    }
    ~Impl(){Stop();}
    void Wake() noexcept {
        revision.fetch_add(1,std::memory_order_release);revision.notify_one();
    }
    void Complete(std::vector<Pending>& batch,const PopulationCheckpoint& checkpoint){
        // A checkpoint string copy can fail after an earlier promise succeeded.
        // Remove each delivered promise immediately so failure cleanup never
        // attempts to satisfy it twice.
        while(!batch.empty()){
            batch.back().promise.set_value(checkpoint);batch.pop_back();
        }
    }
    void Publish(const PopulationSummary& value,double elapsed,bool initial){
        auto health=std::make_shared<PopulationStatus>(*status.load(std::memory_order_acquire));
        health->phase=PopulationPhase::Ready;
        if(initial)health->initializationMs=elapsed;
        else{
            ++health->completedSteps;health->lastStepMs=elapsed;
            health->maxStepMs=std::max(health->maxStepMs,elapsed);
        }
        summary.store(std::make_shared<const PopulationSummary>(value),std::memory_order_release);
        status.store(std::move(health),std::memory_order_release);
    }
    void Work(PopulationBackend& backend,std::vector<Pending>& batch){
        auto started=Clock::now();
        auto current=backend.Initialize();Validate(current);
        if(stopping.load(std::memory_order_acquire))return;
        Publish(current,Milliseconds(started),true);
        while(!stopping.load(std::memory_order_acquire)){
            // Capture before inspecting work. Atomic wait cannot lose a wake
            // between the work check and sleep, without locking the render path.
            const auto observed=revision.load(std::memory_order_acquire);
            std::int64_t nextTarget=0;
            bool idle=false;
            {
                std::lock_guard lock(mutex);
                if(stopping.load(std::memory_order_acquire))break;
                // A checkpoint has a fixed target, so subsequent render updates
                // cannot move its finish line indefinitely.
                nextTarget=pending.empty()?target.load(std::memory_order_acquire):pending.front().target;
                idle=pending.empty()&&(nextTarget<=current.nowMs||nextTarget-current.lastTickMs<5000);
                while(!pending.empty()&&pending.front().target<=current.nowMs){
                    batch.push_back(std::move(pending.front()));pending.pop_front();
                }
            }
            if(idle){revision.wait(observed,std::memory_order_acquire);continue;}
            if(!batch.empty()){
                const auto json=backend.Serialize();
                if(json.empty())throw std::runtime_error("Empty population checkpoint");
                Complete(batch,{true,json,{},current.nowMs});
                continue;
            }
            if(nextTarget<=current.nowMs)continue;
            // Like the original drain, consume the latest target once. The
            // original tick owns elapsed-bucket caps and long-gap recovery.
            // Explicit checkpoints may advance a partial final interval.
            const auto next=nextTarget;
            started=Clock::now();auto advanced=backend.AdvanceTo(next);Validate(advanced);
            if(advanced.nowMs!=next||advanced.lastTickMs<current.lastTickMs)
                throw std::runtime_error("Population backend moved outside its requested clock");
            if(stopping.load(std::memory_order_acquire))break;
            current=std::move(advanced);Publish(current,Milliseconds(started),false);
        }
    }
    void Run() noexcept {
        std::unique_ptr<PopulationBackend> backend;
        std::vector<Pending> batch;
        std::string failure;
        try{
            if(!factory)throw std::runtime_error("Missing population backend factory");
            backend=factory();
            if(!backend)throw std::runtime_error("Population backend factory returned no backend");
            {
                std::lock_guard lock(mutex);activeBackend=backend.get();
                if(stopping.load(std::memory_order_acquire))activeBackend->RequestInterrupt();
            }
            if(!stopping.load(std::memory_order_acquire))Work(*backend,batch);
        }catch(const std::exception& e){failure=e.what();}
        catch(...){failure="Unknown Native population failure";}
        {
            std::lock_guard lock(mutex);
            activeBackend=nullptr;
            auto health=std::make_shared<PopulationStatus>(*status.load(std::memory_order_acquire));
            health->phase=stopping.load(std::memory_order_acquire)?PopulationPhase::Stopped:PopulationPhase::Failed;
            health->error=health->phase==PopulationPhase::Failed?failure:"Population worker stopped";
            status.store(health,std::memory_order_release);
            while(!pending.empty()){batch.push_back(std::move(pending.front()));pending.pop_front();}
            Complete(batch,{false,{},health->error,0});
        }
        // The context and its Native ICU/COM services are destroyed here, on
        // their owner thread, after the borrowed cancellation pointer is cleared.
    }
    void Stop() noexcept {
        std::lock_guard joinLock(joinMutex);
        stopping.store(true,std::memory_order_release);
        {
            std::lock_guard lock(mutex);
            if(activeBackend)activeBackend->RequestInterrupt();
        }
        Wake();
        if(worker.joinable())worker.join();
    }
};

PopulationRuntime::PopulationRuntime(BackendFactory factory):impl_(std::make_unique<Impl>(std::move(factory))){}
PopulationRuntime::~PopulationRuntime()=default;
bool PopulationRuntime::AdvanceTo(std::int64_t time) noexcept {
    if(time<0||time>kMaxJsTime||impl_->stopping.load(std::memory_order_acquire)||
       impl_->status.load(std::memory_order_acquire)->phase==PopulationPhase::Failed)return false;
    auto previous=impl_->target.load(std::memory_order_relaxed);
    while(time>previous){
        if(impl_->target.compare_exchange_weak(previous,time,std::memory_order_release,std::memory_order_relaxed)){
            impl_->Wake();break;
        }
    }
    return true;
}
std::shared_ptr<const PopulationSummary> PopulationRuntime::Snapshot() const noexcept {
    return impl_->summary.load(std::memory_order_acquire);
}
std::shared_ptr<const PopulationStatus> PopulationRuntime::Status() const noexcept {
    return impl_->status.load(std::memory_order_acquire);
}
std::future<PopulationCheckpoint> PopulationRuntime::RequestCheckpoint(){
    Impl::Pending request{impl_->target.load(std::memory_order_acquire),{}};
    auto result=request.promise.get_future();
    {
        std::lock_guard lock(impl_->mutex);
        const auto health=impl_->status.load(std::memory_order_acquire);
        if(impl_->stopping.load(std::memory_order_acquire)||health->phase==PopulationPhase::Failed||
           health->phase==PopulationPhase::Stopped)
            request.promise.set_value({false,{},health->error.empty()?"Population worker stopped":health->error,0});
        else impl_->pending.push_back(std::move(request));
    }
    impl_->Wake();return result;
}
void PopulationRuntime::Stop() noexcept {impl_->Stop();}
}
