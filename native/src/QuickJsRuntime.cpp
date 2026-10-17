#include "pke/QuickJsRuntime.hpp"
#include <quickjs.h>
#include <atomic>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <icu.h>
#else
#include <unicode/ucol.h>
#include <unicode/uloc.h>
#endif

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
struct JsUtf16 {
    JSContext* context;
    const uint16_t* value;
    ~JsUtf16(){if(value)JS_FreeCStringUTF16(context,value);}
};
#ifdef _WIN32
struct CollationCom {
    HRESULT result{CoInitializeEx(nullptr,COINIT_MULTITHREADED)};
    CollationCom(){if(FAILED(result)&&result!=RPC_E_CHANGED_MODE)
        throw std::runtime_error("Could not initialize Native ICU COM services");}
    ~CollationCom(){if(SUCCEEDED(result))CoUninitialize();}
};
#endif
struct NativeCollator {
#ifdef _WIN32
    CollationCom com;
#endif
    std::unique_ptr<UCollator,decltype(&ucol_close)> value{nullptr,&ucol_close};
    explicit NativeCollator(std::string_view requested){
        std::string locale;
        if(requested.empty()){
#ifdef _WIN32
            wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
            const int count=GetUserDefaultLocaleName(name,LOCALE_NAME_MAX_LENGTH);
            if(count>1)for(int i=0;i<count-1;i++)locale+=static_cast<char>(name[i]);
            else locale="en-US";
#else
            locale=uloc_getDefault();
            // Browsers map the POSIX/C host locale to the en-US fallback.
            if(locale=="en_US_POSIX"||locale=="c"||locale=="C"||locale=="POSIX")locale="en_US";
#endif
        }else{
            if(requested.size()>128||requested.find('\0')!=std::string_view::npos)
                throw std::runtime_error("Invalid Native collation language tag");
            const std::string tag(requested);
            char id[ULOC_FULLNAME_CAPACITY]{};
            int32_t parsed=0;UErrorCode status=U_ZERO_ERROR;
            uloc_forLanguageTag(tag.c_str(),id,sizeof(id),&parsed,&status);
            if(U_FAILURE(status)||parsed!=static_cast<int32_t>(tag.size()))
                throw std::runtime_error("Invalid Native collation language tag");
            locale=id;
        }
        UErrorCode status=U_ZERO_ERROR;
        value.reset(ucol_open(locale.c_str(),&status));
        if(U_FAILURE(status)||!value)
            throw std::runtime_error("Could not initialize Native ICU collation");
        // Match the original browser's default sort collator (ECMA-402):
        // canonical normalization on, variant sensitivity, locale punctuation.
        status=U_ZERO_ERROR;
        ucol_setAttribute(value.get(),UCOL_NORMALIZATION_MODE,UCOL_ON,&status);
        ucol_setStrength(value.get(),UCOL_TERTIARY);
        if(U_FAILURE(status))throw std::runtime_error("Could not configure Native ICU collation");
    }
    int Compare(const uint16_t* left,std::size_t leftSize,const uint16_t* right,std::size_t rightSize){
        // Preserve UTF-16 code units, including embedded NUL and lone surrogates.
        const std::vector<UChar> a(left,left+leftSize),b(right,right+rightSize);
        return static_cast<int>(ucol_strcoll(value.get(),a.data(),static_cast<int32_t>(a.size()),
                                             b.data(),static_cast<int32_t>(b.size())));
    }
};
}

struct QuickJsRuntime::Impl {
    using Clock=std::chrono::steady_clock;
    QuickJsLimits limits;
    DiagnosticSink sink;
    const std::thread::id ownerThread;
    JSRuntime* runtime{nullptr};
    JSContext* context{nullptr};
    std::unique_ptr<NativeCollator> collator;
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
        // Some pinned-engine intrinsics report allocation failure through a
        // pending exception even when JS_NewContext returns a pointer.
        if(context && JS_HasException(context)){
            const JSValue exception=JS_GetException(context);
            JS_FreeValue(context,exception);
            JS_FreeContext(context);
            context=nullptr;
        }
        if(!context){
            initializationError="Could not initialize QuickJS context within its limits";
            Report({"initialize",initializationError,{}});
        }else JS_SetContextOpaque(context,this);
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
    static JSValue LocaleCompare(JSContext* context,JSValueConst receiver,int argc,JSValueConst* argv){
        auto* self=static_cast<Impl*>(JS_GetContextOpaque(context));
        if(!self||!self->collator)return JS_ThrowInternalError(context,"Native collator is unavailable");
        if(JS_IsNull(receiver)||JS_IsUndefined(receiver))
            return JS_ThrowTypeError(context,"localeCompare requires a string receiver");
        if((argc>1&&!JS_IsUndefined(argv[1]))||(argc>2&&!JS_IsUndefined(argv[2])))
            return JS_ThrowTypeError(context,"Imported comparisons use the installed Native locale");
        // JS_ToCStringLenUTF16 uses QuickJS's diagnostic string coercion,
        // which may fall back to Error.message. Preserve ordinary ToString
        // exceptions before asking the engine for UTF-16 storage.
        JsValue leftString(context,JS_ToString(context,receiver));
        if(JS_IsException(leftString.value))return JS_EXCEPTION;
        JsValue rightString(context,JS_ToString(context,argc?argv[0]:JS_UNDEFINED));
        if(JS_IsException(rightString.value))return JS_EXCEPTION;
        std::size_t leftSize=0,rightSize=0;
        JsUtf16 left{context,JS_ToCStringLenUTF16(context,&leftSize,leftString.value)};
        if(!left.value)return JS_EXCEPTION;
        JsUtf16 right{context,JS_ToCStringLenUTF16(context,&rightSize,rightString.value)};
        if(!right.value)return JS_EXCEPTION;
        if(leftSize>1024*1024||rightSize>1024*1024)
            return JS_ThrowRangeError(context,"Native compatibility sort key is too long");
        try{return JS_NewInt32(context,self->collator->Compare(left.value,leftSize,right.value,rightSize));}
        catch(const std::exception& e){return JS_ThrowInternalError(context,"%s",e.what());}
        catch(...){return JS_ThrowInternalError(context,"Native collation failed");}
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
QuickJsResult QuickJsRuntime::InstallLocaleCompare(std::string_view locale){
    if(auto failure=impl_->Check("install-locale-compare"))return std::move(*failure);
    Impl::ExecutionScope scope(*impl_);
    std::unique_ptr<NativeCollator> candidate;
    try{candidate=std::make_unique<NativeCollator>(locale);}
    catch(const std::exception& e){return impl_->Failure("install-locale-compare",e.what());}
    JSContext* context=impl_->context;
    JsValue global(context,JS_GetGlobalObject(context));
    JsValue string(context,JS_GetPropertyStr(context,global.value,"String"));
    if(JS_IsException(string.value))return impl_->CaptureException("install-locale-compare");
    JsValue prototype(context,JS_GetPropertyStr(context,string.value,"prototype"));
    if(JS_IsException(prototype.value))return impl_->CaptureException("install-locale-compare");
    JSValue function=JS_NewCFunction(context,&Impl::LocaleCompare,"localeCompare",1);
    if(JS_IsException(function))return impl_->CaptureException("install-locale-compare");
    // DefinePropertyValue consumes the function on both success and failure.
    if(JS_DefinePropertyValueStr(context,prototype.value,"localeCompare",function,
        JS_PROP_WRITABLE|JS_PROP_CONFIGURABLE|JS_PROP_THROW)<0)
        return impl_->CaptureException("install-locale-compare");
    impl_->collator=std::move(candidate);
    return Success();
}
void QuickJsRuntime::RequestInterrupt() noexcept {
    impl_->interrupted.store(true,std::memory_order_relaxed);
}
}
