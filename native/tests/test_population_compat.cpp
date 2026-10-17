#include "TestHarness.hpp"
#include "pke/QuickJsRuntime.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
std::string Read(const std::filesystem::path& path){
    std::ifstream in(path,std::ios::binary);
    if(!in) throw std::runtime_error("Missing compatibility source: "+path.string());
    std::ostringstream out;out<<in.rdbuf();return out.str();
}
void Ok(const pke::QuickJsResult& result){if(!result.ok)throw std::runtime_error(result.error);}
void Load(pke::QuickJsRuntime& runtime){
    Ok(runtime.InstallLocaleCompare("en-US"));
    for(const auto* name:{"cpu-timing.js","title-system.js","population-social-sessions.js",
                         "boss-battle.js","song-difficulty.js","population-imports.js","population.js"})
        Ok(runtime.Evaluate(Read(std::filesystem::path(PKE_COMPAT_DIR)/name),name));
}
void Seed(pke::QuickJsRuntime& runtime){
    // Fixture-only randomness. The shipped runtime never installs this hook.
    Ok(runtime.Evaluate(R"(
        globalThis.fixtureRandomState=123456789;
        Math.random=function(){let t=fixtureRandomState=(fixtureRandomState+0x6D2B79F5)>>>0;
            t=Math.imul(t^(t>>>15),t|1);t^=t+Math.imul(t^(t>>>7),t|61);
            return ((t^(t>>>14))>>>0)/4294967296;};
        function requireCompat(v,message){if(!v)throw Error(message);}
    )"));
}
void Initialize(pke::QuickJsRuntime& runtime){
    Seed(runtime);Load(runtime);
    Ok(runtime.Evaluate("const expectedPopulationFixture="+
        Read(std::filesystem::path(PKE_COMPAT_DIR)/"../tests/fixtures/population/v112-population.json")+";"));
    Ok(runtime.Evaluate(R"(
        function populationFingerprint(value){
            let h=0x811c9dc5;const text=JSON.stringify(value);
            for(let i=0;i<text.length;i++){h^=text.charCodeAt(i);h=Math.imul(h,0x01000193)>>>0;}
            return h.toString(16).padStart(8,'0');
        }
        function requireReferencePopulation(expected){
            requireCompat(JSON.stringify(pkePopulationSummary({}))===JSON.stringify(expected.summary),'reference summary');
            const profiles=pkePopulationQuery({ids:expected.profiles.map(p=>p.profile.id)});
            requireCompat(JSON.stringify(profiles)===JSON.stringify(expected.profiles),'reference profiles and presence transitions');
            const checkpoint=pkePopulationSerialize({});
            requireCompat(populationFingerprint(checkpoint.records)===expected.recordsHash,'reference full-record fingerprint');
            requireCompat(fixtureRandomState===expected.randomState,'reference random call order');
            requireCompat(checkpoint.history.length===expected.historyCount,'reference history');
        }
    )"));
    Ok(runtime.CallJson("pkePopulationInitialize",R"({"now":1791989999000,"songs":[],"skins":[]})"));
}
}

PKE_TEST("Population compatibility initializes all 14999 CPUs and the local player") {
    pke::QuickJsLimits limits;limits.executionBudgetMs=120000;
    pke::QuickJsRuntime runtime(limits);Initialize(runtime);
    const auto summary=runtime.CallJson("pkePopulationSummary","{}");Ok(summary);
    PKE_REQUIRE(summary.json.size()<1024);
    Ok(runtime.Evaluate(R"(
        requireReferencePopulation(expectedPopulationFixture.startup);
        const s=pkePopulationSummary({});
        requireCompat(s.registered===15000 && s.cpuPlayers===14999,'registered population');
        requireCompat(s.live>=1200 && s.live<=2800,'real LIVE rooms');
        const p=pkePopulationQuery({ids:['00001','10001','14999']});
        requireCompat(p.length===3 && p[2].profile.id==='14999','population profiles');
        requireCompat(p[1].profile.careerWins===0 && p[1].profile.careerLosses===0,'new registration');
        pkePopulationInitialize({now:1791989999000,songs:[],skins:['bf','dad'],bannedSkins:['dad'],playerName:'Local',playerSkin:'bf'});
        const bannedState=pkePopulationSerialize({});
        requireCompat(bannedState.records.background.liveMatches.every(room=>
            room.aCharacterId==='bf'&&room.bCharacterId==='bf'),'banned skins excluded from all LIVE rooms');
        requireCompat(!Object.values(bannedState.records.skinById).includes('dad'),'banned skins excluded from stored CPU choices');
        const local=pkePopulationQuery({ids:['YOU']})[0];
        requireCompat(local.profile&&local.profile.isHuman&&local.profile.name==='Local'&&
            local.profile.characterId==='bf'&&local.baseStatus==='YOU','local player query');
    )"));
}

PKE_TEST("Population compatibility advances a minute without changing the local Online record") {
    pke::QuickJsLimits limits;limits.executionBudgetMs=120000;
    pke::QuickJsRuntime runtime(limits);Initialize(runtime);
    Ok(runtime.Evaluate(R"(
        const before=pkePopulationSerialize({});
        pkePopulationAdvanceTo({now:1791990059000});
        requireReferencePopulation(expectedPopulationFixture.oneMinute);
        requireCompat(expectedPopulationFixture.transitions.some(t=>t.to==='SLEEP'),'sleep transition fixture');
        requireCompat(expectedPopulationFixture.transitions.some(t=>t.to==='REST'),'rest transition fixture');
        const after=pkePopulationSerialize({});
        requireCompat(after.now-before.now===60000,'minute elapsed');
        requireCompat(after.records.background.lastTick===after.now,'last population tick');
        requireCompat(Object.keys(after.records.leagueById).length>64,'real CPU results');
        requireCompat(JSON.stringify(before.records.player)===JSON.stringify(after.records.player),'Online player record');
        const now=after.now;pkePopulationAdvanceTo({now:now-10000});
        requireCompat(pkePopulationSummary({}).now===now,'monotonic clock');
    )"));
}

PKE_TEST("Population difficulty helpers retain the original authored-note ratings") {
    pke::QuickJsRuntime runtime;Seed(runtime);Load(runtime);
    Ok(runtime.Evaluate("const referenceDifficultyCases="+
        Read(std::filesystem::path(PKE_COMPAT_DIR)/"../tests/fixtures/population/v112-difficulty.json")+";"));
    Ok(runtime.Evaluate(R"(
        for(const item of referenceDifficultyCases){
            const actual=pkeSongDifficulty(item.input);
            requireCompat(JSON.stringify(actual)===JSON.stringify(item.expected),'reference difficulty: '+item.name);
        }
    )"));
}

PKE_TEST("Population title availability retains earned and dynamic titles") {
    pke::QuickJsRuntime runtime;Seed(runtime);Load(runtime);
    Ok(runtime.Evaluate("const referenceTitleCases="+
        Read(std::filesystem::path(PKE_COMPAT_DIR)/"../tests/fixtures/population/v112-titles.json")+";"));
    Ok(runtime.Evaluate(R"(
        for(const item of referenceTitleCases)
            requireCompat(JSON.stringify(pkeTitleAvailability(item.input))===JSON.stringify(item.expected),'reference titles');
    )"));
}

PKE_TEST("Population weekly songs retain browser collation for mixed-case folder ids") {
    pke::QuickJsRuntime runtime;Seed(runtime);Load(runtime);
    Ok(runtime.Evaluate(R"(
        const songs=['a','B','c','D','e','F'].map(id=>({id,name:id,stars:6,eligible:true}));
        const selected=PKEBossBattle.selectWeeklySongs(songs,Date.UTC(2026,9,16,12)).songs.map(s=>s.id);
        requireCompat(JSON.stringify(selected)===JSON.stringify(['B','F','a']),'reference weekly mixed-case ordering');
    )"));
}

PKE_TEST("Population LIVE authored-note progress and completions match the original calculations") {
    pke::QuickJsLimits limits;limits.executionBudgetMs=120000;
    pke::QuickJsRuntime runtime(limits);Seed(runtime);Load(runtime);
    Ok(runtime.Evaluate("const catalogFixture="+
        Read(std::filesystem::path(PKE_COMPAT_DIR)/"../tests/fixtures/population/v112-catalog.json")+";"));
    Ok(runtime.Evaluate("pkePopulationInitialize(catalogFixture.input);"));
    // Match the worker contract: bound each VM call to one reference tick.
    // Initialization plus all six ticks must not share one execution deadline.
    for(unsigned tick=1;tick<=6;tick++)
        Ok(runtime.Evaluate("pkePopulationAdvanceTo({now:catalogFixture.input.now+"+
                            std::to_string(tick*5000)+"});"));
    Ok(runtime.Evaluate(R"(
        const actual=pkePopulationSerialize({}),expected=catalogFixture.expected;
        requireCompat(JSON.stringify(pkePopulationSummary({}))===JSON.stringify(expected.summary),'catalog summary');
        let h=0x811c9dc5;const text=JSON.stringify(actual.records);
        for(let i=0;i<text.length;i++){h^=text.charCodeAt(i);h=Math.imul(h,0x01000193)>>>0;}
        requireCompat(h.toString(16).padStart(8,'0')===expected.recordsHash,'catalog full-record fingerprint');
        requireCompat(fixtureRandomState===expected.randomState,'catalog RNG order');
        requireCompat(actual.history.length===expected.historyCount,'catalog history');
        requireCompat(actual.history.filter(m=>m.source==='live').length===expected.liveCompletions,'actual LIVE completions');
        requireCompat(expected.liveCompletions>0,'fixture includes completed LIVE notes');
        requireCompat(JSON.stringify(actual.records.background.liveMatches.slice(0,3))===JSON.stringify(expected.roomSamples),'live cursors and judgements');
    )"));
}

PKE_TEST("Population serialization restores equivalent summaries records and presence") {
    pke::QuickJsLimits limits;limits.executionBudgetMs=120000;
    pke::QuickJsRuntime runtime(limits);Initialize(runtime);
    Ok(runtime.Evaluate(R"(
        pkePopulationAdvanceTo({now:1791990059000});
        const saved=pkePopulationSerialize({});
        const summary=JSON.stringify(pkePopulationSummary({}));
        const players=JSON.stringify(pkePopulationQuery({ids:['00001','04242','14999']}));
        pkePopulationInitialize({now:saved.now+30000,songs:[],skins:[]});
        pkePopulationRestore(saved);
        requireCompat(JSON.stringify(pkePopulationSummary({}))===summary,'restored summary');
        requireCompat(JSON.stringify(pkePopulationQuery({ids:['00001','04242','14999']}))===players,'restored profiles and presence');
        requireCompat(JSON.stringify(pkePopulationSerialize({}))===JSON.stringify(saved),'restored checkpoint');
    )"));
}

PKE_TEST("Population Halloween pure state follows exact JST boundaries and Native overrides") {
    pke::QuickJsRuntime runtime;Seed(runtime);Load(runtime);
    Ok(runtime.Evaluate(R"(
        const start=Date.UTC(2026,9,14,15),end=Date.UTC(2026,9,31,15);
        requireCompat(!pkeHalloweenState({now:start-1}).active,'before start');
        requireCompat(pkeHalloweenState({now:start}).active,'inclusive start');
        requireCompat(pkeHalloweenState({now:end-1}).active,'before end');
        requireCompat(!pkeHalloweenState({now:end}).active,'exclusive end');
        requireCompat(pkeHalloweenState({now:start-1,override:'force-on'}).active,'force on');
        requireCompat(!pkeHalloweenState({now:start,override:'force-off'}).active,'force off');
        requireCompat(pkeHalloweenState({now:start}).teamTarget===255000,'V1.14 Halloween TEAM');
    )"));
}
