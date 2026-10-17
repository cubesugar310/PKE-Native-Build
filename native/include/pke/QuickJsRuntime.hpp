#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace pke {
struct QuickJsLimits {
    std::size_t memoryBytes{512u*1024u*1024u};
    std::size_t stackBytes{256u*1024u};
    std::uint32_t executionBudgetMs{5000};
};
struct QuickJsDiagnostic {
    std::string operation;
    std::string message;
    std::string stack;
};
struct QuickJsResult {
    bool ok{false};
    std::string json;
    std::string error;
};

// Create, use, and destroy on one owning thread. No host filesystem, browser
// objects, module loader, or application timers are installed in the context.
class QuickJsRuntime {
public:
    using DiagnosticSink=std::function<void(const QuickJsDiagnostic&)>;
    explicit QuickJsRuntime(QuickJsLimits limits={},DiagnosticSink sink={});
    ~QuickJsRuntime();
    QuickJsRuntime(const QuickJsRuntime&)=delete;
    QuickJsRuntime& operator=(const QuickJsRuntime&)=delete;

    bool Ready() const noexcept;
    QuickJsResult Evaluate(std::string_view source,
                           std::string_view filename="native-compat.js");
    QuickJsResult CallJson(std::string_view functionName,std::string_view inputJson);
    QuickJsResult CollectGarbage();
    // Thread safe. A requested interruption is permanent for this instance.
    void RequestInterrupt() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
