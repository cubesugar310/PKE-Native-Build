#include "pke/QuickJsRuntime.hpp"
#include <quickjs.h>
#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <utility>

namespace pke {
namespace {
struct JsValue {
    JSContext* context;
    JSValue value;
    JsValue(JSContext* c,JSValue v):context(c),value(v){}
    ~JsValue(){ JS_FreeValue(context,value); }
    JsValue(const JsValue&)=delete;
    JsValue& operator=(const JsValue&)=delete;
};
struct JsCString {
    JSContext* context;
    const char* value;
    ~JsCString(){ if(value) JS_FreeCString(context,value); }
};
QuickJsResult Success(std::string json={}) { return {true,std::move(json),{}}; }
}

struct QuickJsRuntime::Impl {
    using Clock=std::chrono::steady_clock;
    QuickJsLimits limits;
    DiagnosticSink sink;
    const std::thread::id ownerThread;
    JSRuntime* runtime{nullptr};
    JSContext* context{nullptr};
    Clock::time_point deadline{Clock::time_point::max()};
    std::atomic<bool> interrupted{false};
    std::string initializationError;

    Impl(QuickJsLimits value,DiagnosticSink callback)
        :limits(value),sink(std::move(callback)),ownerThread(std::this_thread::get_id()){
        if(!limits.memoryBytes || !limits.stackBytes || !limits.executionBudgetMs){
            initializationError="QuickJS limits must be positive";
            Report({"initialize",initializationError,{}});
            return;
        }
        runtime=JS_NewRuntime();
        if(!runtime){
            initializationError="Could not allocate QuickJS runtime";
            Report({"initialize",initializationError,{}});
            return;
        }
        JS_SetMemoryLimit(runtime,limits.memoryBytes);
        JS_SetMaxStackSize(runtime,limits.stackBytes);
        JS_SetCanBlock(runtime,false);
        JS_SetInterruptHandler(runtime,&Impl::Interrupt,this);
        context=JS_NewContext(runtime);
        if(!context){
            initializationError="Could not initialize QuickJS context within its limits";
            Report({"initialize",initializationError,{}});
        }
    }
    ~Impl(){
        if(context) JS_FreeContext(context);
        if(runtime) JS_FreeRuntime(runtime);
    }
    static int Interrupt(JSRuntime*,void* opaque) noexcept {
        auto& self=*static_cast<Impl*>(opaque);
        return self.interrupted.load(std::memory_order_relaxed) ||
               Clock::now()>=self.deadline;
    }
    struct ExecutionScope {
        Impl& impl;
        explicit ExecutionScope(Impl& value):impl(value){
            impl.deadline=Clock::now()+std::chrono::milliseconds(impl.limits.executionBudgetMs);
        }
        ~ExecutionScope(){ impl.deadline=Clock::time_point::max(); }
    };
    void Report(const QuickJsDiagnostic& diagnostic) noexcept {
        if(!sink) return;
        // Diagnostics must not turn a recoverable JS error into a Native crash.
        try { sink(diagnostic); } catch(...) {}
    }
    QuickJsResult Failure(std::string operation,std::string message,
                          std::string stack={},bool report=true){
        QuickJsDiagnostic diagnostic{std::move(operation),std::move(message),std::move(stack)};
        std::string error=diagnostic.operation+": "+diagnostic.message;
        if(!diagnostic.stack.empty()) error+="\n"+diagnostic.stack;
        if(report) Report(diagnostic);
        return {false,{},std::move(error)};
    }
    std::optional<QuickJsResult> Check(std::string_view operation){
        // The foreign-thread path must neither touch QuickJS nor invoke the
        // owner's diagnostics callback.
        if(std::this_thread::get_id()!=ownerThread)
            return Failure(std::string(operation),"QuickJS called from the wrong Native thread",{},false);
        if(!context)
            return Failure(std::string(operation),initializationError.empty()
                ? "QuickJS context is not initialized" : initializationError);
        if(interrupted.load(std::memory_order_relaxed))
            return Failure(std::string(operation),"QuickJS interrupted by Native host");
        return std::nullopt;
    }
    void DrainException(){
        JsValue ignored(context,JS_GetException(context));
    }
    std::string DiagnosticText(JSValueConst value){
        std::size_t length=0;
        JsCString text{context,JS_ToCStringLen(context,&length,value)};
        if(!text.value){ DrainException(); return {}; }
        return std::string(text.value,length);
    }
    QuickJsResult CaptureException(std::string_view operation){
        JsValue exception(context,JS_GetException(context));
        std::string message=DiagnosticText(exception.value);
        if(message.empty()) message="JavaScript exception";
        std::string stack;
        if(JS_IsObject(exception.value)){
            JsValue trace(context,JS_GetPropertyStr(context,exception.value,"stack"));
            if(JS_IsException(trace.value)) DrainException();
            else if(!JS_IsUndefined(trace.value) && !JS_IsNull(trace.value))
                stack=DiagnosticText(trace.value);
        }
        return Failure(std::string(operation),std::move(message),std::move(stack));
    }
};

QuickJsRuntime::QuickJsRuntime(QuickJsLimits limits,DiagnosticSink sink)
    :impl_(std::make_unique<Impl>(limits,std::move(sink))){}
QuickJsRuntime::~QuickJsRuntime()=default;
bool QuickJsRuntime::Ready() const noexcept { return impl_->context!=nullptr; }

QuickJsResult QuickJsRuntime::Evaluate(std::string_view source,std::string_view filename){
    const std::string operation="evaluate "+std::string(filename);
    if(auto failure=impl_->Check(operation)) return std::move(*failure);
    Impl::ExecutionScope scope(*impl_);
    // QuickJS expects the byte span to have a terminating NUL.
    const std::string code(source),name(filename);
    JsValue value(impl_->context,JS_Eval(impl_->context,code.c_str(),code.size(),
                                       name.c_str(),JS_EVAL_TYPE_GLOBAL));
    if(JS_IsException(value.value)) return impl_->CaptureException(operation);
    return Success();
}

QuickJsResult QuickJsRuntime::CallJson(std::string_view functionName,std::string_view inputJson){
    const std::string operation(functionName);
    if(auto failure=impl_->Check(operation)) return std::move(*failure);
    if(operation.empty() || operation.find('\0')!=std::string::npos)
        return impl_->Failure(operation,"Invalid compatibility function name");
    Impl::ExecutionScope scope(*impl_);
    JSContext* context=impl_->context;
    JsValue global(context,JS_GetGlobalObject(context));
    JsValue function(context,JS_GetPropertyStr(context,global.value,operation.c_str()));
    if(JS_IsException(function.value)) return impl_->CaptureException(operation);
    if(!JS_IsFunction(context,function.value))
        return impl_->Failure(operation,"Compatibility function not found or not callable");
    const std::string input(inputJson);
    JsValue argument(context,JS_ParseJSON(context,input.c_str(),input.size(),"<native-json>"));
    if(JS_IsException(argument.value)) return impl_->CaptureException(operation);
    JSValueConst arguments[]{argument.value};
    JsValue result(context,JS_Call(context,function.value,global.value,1,arguments));
    if(JS_IsException(result.value)) return impl_->CaptureException(operation);
    if(JS_IsPromise(result.value))
        return impl_->Failure(operation,"Compatibility functions must return JSON synchronously");
    JsValue serialized(context,JS_JSONStringify(context,result.value,JS_UNDEFINED,JS_UNDEFINED));
    if(JS_IsException(serialized.value)) return impl_->CaptureException(operation);
    if(JS_IsUndefined(serialized.value))
        return impl_->Failure(operation,"Compatibility function result is not JSON serializable");
    std::size_t length=0;
    JsCString json{context,JS_ToCStringLen(context,&length,serialized.value)};
    if(!json.value) return impl_->CaptureException(operation);
    return Success(std::string(json.value,length));
}

QuickJsResult QuickJsRuntime::CollectGarbage(){
    if(auto failure=impl_->Check("collect-garbage")) return std::move(*failure);
    Impl::ExecutionScope scope(*impl_);
    JS_RunGC(impl_->runtime);
    return Success();
}
void QuickJsRuntime::RequestInterrupt() noexcept {
    impl_->interrupted.store(true,std::memory_order_relaxed);
}
}
