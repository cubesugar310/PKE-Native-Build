#include "TestHarness.hpp"
#include "pke/PopulationRuntime.hpp"
#include <chrono>
#include <thread>
using namespace std::chrono_literals;
PKE_TEST("Population startup waits for Native catalog data and can be cancelled") {
    std::promise<std::string> data;pke::PopulationRuntimeOptions options;
    options.compatibilityDirectory=PKE_COMPAT_DIR;options.initializeJsonFuture=data.get_future().share();
    pke::PopulationRuntime runtime(std::move(options));std::this_thread::sleep_for(150ms);
    PKE_REQUIRE(runtime.Status()->phase==pke::PopulationPhase::Starting);PKE_REQUIRE(!runtime.Snapshot());
    const auto start=std::chrono::steady_clock::now();runtime.Stop();
    PKE_REQUIRE(std::chrono::steady_clock::now()-start<2s);
}
PKE_TEST("Population worker consumes the catalog future instead of a stale initialization string") {
    std::promise<std::string> data;pke::PopulationRuntimeOptions options;
    options.compatibilityDirectory=PKE_COMPAT_DIR;options.initializeJson=R"({"now":1791989999000,"songs":[],"skins":[]})";
    options.initializeJsonFuture=data.get_future().share();data.set_value("{broken-catalog");
    pke::PopulationRuntime runtime(std::move(options));const auto end=std::chrono::steady_clock::now()+5s;
    while(runtime.Status()->phase==pke::PopulationPhase::Starting&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(1ms);
    PKE_REQUIRE(runtime.Status()->phase==pke::PopulationPhase::Failed);PKE_REQUIRE(!runtime.Snapshot());
}
