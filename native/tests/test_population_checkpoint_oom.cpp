#include "pke/PopulationRuntime.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <new>

namespace {
constexpr std::size_t kCheckpointSize=65536;
std::atomic<int> matchingAllocations{0};
}
// Isolate the recoverable allocation fault from all other test executables.
// Only the checkpoint-sized string allocation is affected, exactly once.
void* operator new(std::size_t size){
    // libstdc++ requests size+1; MSVC may round string capacity to 16 bytes.
    if(size>kCheckpointSize&&size<=kCheckpointSize+64){
        auto left=matchingAllocations.load();
        while(left>0){
            if(matchingAllocations.compare_exchange_weak(left,left-1)){
                if(left==1)throw std::bad_alloc();
                break;
            }
        }
    }
    if(void* value=std::malloc(size?size:1))return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept {std::free(value);}
void operator delete(void* value,std::size_t) noexcept {std::free(value);}

namespace {
struct Gate {std::mutex mutex;std::condition_variable cv;bool released=false;};
class Backend final:public pke::PopulationBackend {
public:
    explicit Backend(std::shared_ptr<Gate> gate):gate_(std::move(gate)){}
    pke::PopulationSummary Initialize() override{
        std::unique_lock lock(gate_->mutex);gate_->cv.wait(lock,[&]{return gate_->released;});
        pke::PopulationSummary s;s.registered=15000;s.cpuPlayers=14999;
        s.nowMs=s.lastTickMs=1791989999000;s.localPlayerState="YOU";return s;
    }
    pke::PopulationSummary AdvanceTo(std::int64_t) override{throw std::runtime_error("unexpected step");}
    std::string Serialize() override{
        std::string json="{\"data\":\"";json.append(kCheckpointSize-11,'x');json+="\"}";
        if(json.size()!=kCheckpointSize)throw std::runtime_error("invalid fault fixture size");
        // One copy creates the batch value, then the two promise deliveries.
        matchingAllocations.store(3);return json;
    }
    void RequestInterrupt() noexcept override{
        std::lock_guard lock(gate_->mutex);gate_->released=true;gate_->cv.notify_all();
    }
private:
    std::shared_ptr<Gate> gate_;
};
}
int main(){
    std::set_terminate([]{std::_Exit(42);});
    auto gate=std::make_shared<Gate>();
    pke::PopulationRuntime runtime([gate]{return std::make_unique<Backend>(gate);});
    auto first=runtime.RequestCheckpoint(),second=runtime.RequestCheckpoint();
    {std::lock_guard lock(gate->mutex);gate->released=true;gate->cv.notify_all();}
    using namespace std::chrono_literals;
    if(first.wait_for(5s)!=std::future_status::ready||second.wait_for(5s)!=std::future_status::ready)return 1;
    const auto a=first.get(),b=second.get();
    if(a.ok==b.ok||runtime.Status()->phase!=pke::PopulationPhase::Failed)return 2;
    const auto& failed=a.ok?b:a;const auto& saved=a.ok?a:b;
    if(!failed.json.empty()||failed.error.empty()||saved.json.size()!=kCheckpointSize)return 3;
    if(!runtime.Snapshot()||runtime.Snapshot()->registered!=15000)return 4;
    auto after=runtime.RequestCheckpoint();
    if(after.wait_for(1s)!=std::future_status::ready||after.get().ok)return 5;
    std::cout<<"[PASS] recoverable checkpoint-copy allocation failure resolves remaining futures\n";
}
