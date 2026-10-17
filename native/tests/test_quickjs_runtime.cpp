#include "TestHarness.hpp"
#include "pke/QuickJsRuntime.hpp"
#include <quickjs.h>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

PKE_TEST("QuickJS compatibility calls round-trip nested JSON without changing player ids") {
    pke::QuickJsRuntime runtime;
    PKE_REQUIRE(runtime.Ready());
    PKE_REQUIRE(runtime.Evaluate("function nativeEcho(value){ return value; }").ok);
    const std::string input=R"({"id":"00001","n":3.25,"nested":[true,null,{"x":"line\\nend"}]})";
    const auto result=runtime.CallJson("nativeEcho",input);
    PKE_REQUIRE(result.ok);
    PKE_REQUIRE(result.json==input);
}

PKE_TEST("QuickJS compatibility context has no browser or filesystem globals") {
    pke::QuickJsRuntime runtime;
    PKE_REQUIRE(runtime.Evaluate(R"(
        function nativeGlobals() {
            return [typeof document,typeof window,typeof HTMLCanvasElement,
                typeof AudioContext,typeof localStorage,typeof indexedDB,
                typeof setTimeout,typeof setInterval,typeof require,typeof std,typeof os];
        }
    )").ok);
    const auto result=runtime.CallJson("nativeGlobals","{}");
    PKE_REQUIRE(result.ok);
    PKE_REQUIRE(result.json==R"(["undefined","undefined","undefined","undefined","undefined","undefined","undefined","undefined","undefined","undefined","undefined"])");
}

PKE_TEST("QuickJS exceptions report their source stack to Native diagnostics") {
    std::vector<pke::QuickJsDiagnostic> diagnostics;
    pke::QuickJsRuntime runtime({},[&](const pke::QuickJsDiagnostic& d){ diagnostics.push_back(d); });
    PKE_REQUIRE(runtime.Evaluate(
        "function nativeCrash(){ throw new Error('sentinel-boom'); }","compat-test.js").ok);
    const auto result=runtime.CallJson("nativeCrash","{}");
    PKE_REQUIRE(!result.ok);
    PKE_REQUIRE(result.error.find("sentinel-boom")!=std::string::npos);
    PKE_REQUIRE(result.error.find("compat-test.js")!=std::string::npos);
    PKE_REQUIRE(diagnostics.size()==1);
    PKE_REQUIRE(diagnostics[0].operation=="nativeCrash");
    PKE_REQUIRE(diagnostics[0].stack.find("compat-test.js")!=std::string::npos);
    PKE_REQUIRE(runtime.Evaluate("1+1").ok);
}

PKE_TEST("QuickJS memory limit failure is captured and the process remains usable") {
    pke::QuickJsLimits limits;
    limits.memoryBytes=8*1024*1024;
    pke::QuickJsRuntime runtime(limits);
    PKE_REQUIRE(runtime.Ready());
    const auto result=runtime.Evaluate("new Array(32*1024*1024).fill(7);","memory-test.js");
    PKE_REQUIRE(!result.ok);
    PKE_REQUIRE(!result.error.empty());
    PKE_REQUIRE(runtime.CollectGarbage().ok);
    PKE_REQUIRE(runtime.Evaluate("1+1").ok);
}

PKE_TEST("QuickJS stack limit rejects recursion without crashing the process") {
    pke::QuickJsLimits limits;
    limits.stackBytes=64*1024;
    pke::QuickJsRuntime runtime(limits);
    PKE_REQUIRE(runtime.Ready());
    const auto result=runtime.Evaluate(
        "function nativeRecurse(){ return 1+nativeRecurse(); } nativeRecurse();","stack-test.js");
    PKE_REQUIRE(!result.ok);
    PKE_REQUIRE(!result.error.empty());
    PKE_REQUIRE(runtime.CollectGarbage().ok);
    PKE_REQUIRE(runtime.Evaluate("1+1").ok);
}

PKE_TEST("QuickJS execution budget interrupts a nonterminating compatibility function") {
    pke::QuickJsLimits limits;
    limits.executionBudgetMs=20;
    pke::QuickJsRuntime runtime(limits);
    const auto started=std::chrono::steady_clock::now();
    const auto result=runtime.Evaluate("while(true){}","budget-test.js");
    const auto elapsed=std::chrono::steady_clock::now()-started;
    PKE_REQUIRE(!result.ok);
    PKE_REQUIRE(elapsed<std::chrono::seconds(2));
}

PKE_TEST("QuickJS bridge rejects invalid JSON missing functions and non-JSON results") {
    pke::QuickJsRuntime runtime;
    PKE_REQUIRE(runtime.Evaluate(
        "function nativeEcho(x){return x;} function nativeUndefined(){return undefined;}").ok);
    PKE_REQUIRE(!runtime.CallJson("nativeEcho","{broken").ok);
    PKE_REQUIRE(!runtime.CallJson("missing","{}").ok);
    PKE_REQUIRE(!runtime.CallJson("nativeUndefined","{}").ok);
    PKE_REQUIRE(runtime.CallJson("nativeEcho",R"({"ok":true})").ok);
}

PKE_TEST("QuickJS explicit garbage collection preserves reachable compatibility state") {
    pke::QuickJsRuntime runtime;
    PKE_REQUIRE(runtime.Evaluate(
        "globalThis.kept={id:'00001'}; function nativeKept(){return kept;}").ok);
    PKE_REQUIRE(runtime.CollectGarbage().ok);
    const auto result=runtime.CallJson("nativeKept","{}");
    PKE_REQUIRE(result.ok);
    PKE_REQUIRE(result.json==R"({"id":"00001"})");
}

PKE_TEST("QuickJS contexts reject calls from a different Native thread") {
    pke::QuickJsRuntime runtime;
    PKE_REQUIRE(runtime.Evaluate("function nativeEcho(x){return x;}").ok);
    pke::QuickJsResult result;
    pke::QuickJsResult collation;
    std::thread other([&]{ result=runtime.CallJson("nativeEcho","{}");
        collation=runtime.InstallLocaleCompare("en-US"); });
    other.join();
    PKE_REQUIRE(!result.ok);
    PKE_REQUIRE(result.error.find("thread")!=std::string::npos);
    PKE_REQUIRE(!collation.ok);
    PKE_REQUIRE(collation.error.find("thread")!=std::string::npos);
    PKE_REQUIRE(runtime.CallJson("nativeEcho","{}").ok);
}

PKE_TEST("QuickJS Native interrupt request is thread safe and remains effective") {
    pke::QuickJsRuntime runtime;
    const auto started=std::chrono::steady_clock::now();
    std::thread other([&]{ runtime.RequestInterrupt(); });
    const auto result=runtime.Evaluate("while(true){}","cancel-test.js");
    other.join();
    PKE_REQUIRE(!result.ok);
    PKE_REQUIRE(std::chrono::steady_clock::now()-started<std::chrono::seconds(2));
    PKE_REQUIRE(!runtime.Evaluate("1+1").ok);
}

PKE_TEST("QuickJS context initialization failure is a normal Native error") {
    pke::QuickJsLimits limits;
    limits.memoryBytes=1024;
    pke::QuickJsRuntime runtime(limits);
    PKE_REQUIRE(!runtime.Ready());
    const auto result=runtime.Evaluate("1+1");
    PKE_REQUIRE(!result.ok);
    PKE_REQUIRE(!result.error.empty());
}

PKE_TEST("QuickJS rejects partially initialized contexts after intrinsic allocation failure") {
    pke::QuickJsLimits limits;
    // Find the engine's actual partial-intrinsic boundary. Allocator overhead
    // differs between MSVC and GCC, so a fixed byte count is not portable.
    std::size_t partialBudget=0;
    for(std::size_t bytes=32*1024;bytes<256*1024&&!partialBudget;bytes+=256){
        JSRuntime* probe=JS_NewRuntime();
        PKE_REQUIRE(probe!=nullptr);
        JS_SetMemoryLimit(probe,bytes);
        JSContext* context=JS_NewContext(probe);
        if(context){
            if(JS_HasException(context)){
                partialBudget=bytes;
                const JSValue exception=JS_GetException(context);
                JS_FreeValue(context,exception);
            }
            JS_FreeContext(context);
        }
        JS_FreeRuntime(probe);
    }
    PKE_REQUIRE(partialBudget!=0);
    limits.memoryBytes=partialBudget;
    std::vector<pke::QuickJsDiagnostic> diagnostics;
    pke::QuickJsRuntime runtime(limits,[&](const auto& d){diagnostics.push_back(d);});
    PKE_REQUIRE(!runtime.Ready());
    PKE_REQUIRE(!diagnostics.empty());
    PKE_REQUIRE(diagnostics.front().operation=="initialize");
    PKE_REQUIRE(!runtime.Evaluate("1+1").ok);
}

PKE_TEST("QuickJS Native collation preserves browser case ordering without browser globals") {
    pke::QuickJsRuntime runtime;
    PKE_REQUIRE(runtime.InstallLocaleCompare("en-US").ok);
    PKE_REQUIRE(runtime.Evaluate(R"(
        function nativeCollation(){return ['a','B','c','D','e','F'].sort((a,b)=>a.localeCompare(b));}
    )").ok);
    const auto result=runtime.CallJson("nativeCollation","{}");
    PKE_REQUIRE(result.ok);
    PKE_REQUIRE(result.json==R"(["a","B","c","D","e","F"])");
    PKE_REQUIRE(!runtime.InstallLocaleCompare("not a language tag").ok);
    PKE_REQUIRE(runtime.CollectGarbage().ok);
    PKE_REQUIRE(runtime.CallJson("nativeCollation","{}").json==result.json);
    PKE_REQUIRE(runtime.Evaluate(R"(
        if(typeof document!=='undefined'||typeof window!=='undefined'||typeof setTimeout!=='undefined')throw Error('browser global');
        if('e\u0301'.localeCompare('\u00e9')!==0)throw Error('canonical equivalence');
        if('\u0000a'.localeCompare('\u0000b')>=0)throw Error('embedded NUL');
        '\ud800'.localeCompare('\ud801');
    )").ok);
    PKE_REQUIRE(runtime.Evaluate("Object.freeze(String.prototype)").ok);
    PKE_REQUIRE(!runtime.InstallLocaleCompare("ja-JP").ok);
    PKE_REQUIRE(runtime.CallJson("nativeCollation","{}").json==result.json);
}

PKE_TEST("QuickJS Native collation propagates operand string conversion exceptions") {
    pke::QuickJsRuntime runtime;
    PKE_REQUIRE(runtime.InstallLocaleCompare("en-US").ok);
    for(const auto* expression:{"String.prototype.localeCompare.call(bad,'x')",
                               "'x'.localeCompare(bad)"}){
        const std::string source=std::string(R"(
            var bad=new Error('fallback');
            bad.toString=()=>{throw Error('sentinel-to-string');};
        )")+expression;
        const auto result=runtime.Evaluate(source,"collation-coercion.js");
        PKE_REQUIRE(!result.ok);
        PKE_REQUIRE(result.error.find("sentinel-to-string")!=std::string::npos);
        PKE_REQUIRE(runtime.Evaluate("'a'.localeCompare('b')").ok);
    }
}
