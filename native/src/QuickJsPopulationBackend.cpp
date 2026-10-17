#include "pke/PopulationRuntime.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <atomic>
#include <chrono>
#ifdef PKE_NATIVE_USE_NLOHMANN
#include <nlohmann/json.hpp>
#else
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#endif

namespace pke {
namespace {
std::string Require(QuickJsResult result){
    if(!result.ok)throw std::runtime_error(result.error);
    return std::move(result.json);
}
PopulationSummary ParseSummary(const std::string& json){
    PopulationSummary value;
#ifdef PKE_NATIVE_USE_NLOHMANN
    const auto object=nlohmann::json::parse(json);
    value.registered=object.at("registered").get<int>();
    value.cpuPlayers=object.at("cpuPlayers").get<int>();
    value.nowMs=object.at("now").get<std::int64_t>();
    value.lastTickMs=object.at("lastTick").get<std::int64_t>();
    value.online=object.at("online").get<int>();
    value.live=object.at("live").get<int>();
    value.liveCap=object.at("liveCap").get<int>();
    value.waiting=object.at("waiting").get<int>();
    value.localPlayerState=object.at("localPlayerState").get<std::string>();
#else
    boost::property_tree::ptree object;
    std::istringstream input(json);boost::property_tree::read_json(input,object);
    value.registered=object.get<int>("registered");
    value.cpuPlayers=object.get<int>("cpuPlayers");
    value.nowMs=object.get<std::int64_t>("now");
    value.lastTickMs=object.get<std::int64_t>("lastTick");
    value.online=object.get<int>("online");
    value.live=object.get<int>("live");
    value.liveCap=object.get<int>("liveCap");
    value.waiting=object.get<int>("waiting");
    value.localPlayerState=object.get<std::string>("localPlayerState");
#endif
    return value;
}
std::string ReadSource(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Missing population compatibility script: "+path.string());
    std::ostringstream content;content<<input.rdbuf();
    if(input.bad())throw std::runtime_error("Could not read population compatibility script: "+path.string());
    return content.str();
}
class QuickJsPopulationBackend final:public PopulationBackend {
public:
    explicit QuickJsPopulationBackend(PopulationRuntimeOptions options)
        :options_(std::move(options)),runtime_(options_.limits){}
    PopulationSummary Initialize() override{
        Require(runtime_.InstallLocaleCompare(options_.locale));
        for(const auto* name:{"cpu-timing.js","title-system.js","population-social-sessions.js",
                             "boss-battle.js","song-difficulty.js","population-imports.js","population.js"})
            Require(runtime_.Evaluate(ReadSource(options_.compatibilityDirectory/name),name));
        if(options_.initializeJsonFuture.valid()){
            while(options_.initializeJsonFuture.wait_for(std::chrono::milliseconds(5))!=std::future_status::ready)
                if(cancelled_.load())throw std::runtime_error("Population catalog wait cancelled");
            options_.initializeJson=options_.initializeJsonFuture.get();
        }
        if(options_.checkpointJson.empty())
            return ParseSummary(Require(runtime_.CallJson("pkePopulationInitialize",options_.initializeJson)));
        // Fresh process restore keeps the supplied catalog and reconstructs
        // derived caches once; it does not first simulate a discarded world.
        return ParseSummary(Require(runtime_.CallJson("pkePopulationRestoreWithOptions",
            "{\"options\":"+options_.initializeJson+",\"checkpoint\":"+options_.checkpointJson+"}")));
    }
    PopulationSummary AdvanceTo(std::int64_t time) override{
        return ParseSummary(Require(runtime_.CallJson("pkePopulationAdvanceTo",
            "{\"now\":"+std::to_string(time)+"}")));
    }
    std::string Serialize() override{
        return Require(runtime_.CallJson("pkePopulationSerialize","{}"));
    }
    void RequestInterrupt() noexcept override{cancelled_.store(true);runtime_.RequestInterrupt();}
private:
    PopulationRuntimeOptions options_;
    QuickJsRuntime runtime_;
    std::atomic<bool> cancelled_{};
};
}
PopulationRuntime::PopulationRuntime(PopulationRuntimeOptions options)
    :PopulationRuntime([options=std::move(options)]() mutable{
        return std::make_unique<QuickJsPopulationBackend>(std::move(options));
    }){}
}
