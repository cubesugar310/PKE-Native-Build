#include "pke/AtomicSaveFile.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace pke {
namespace {
constexpr std::uintmax_t kMaximumSaveBytes=64u*1024u*1024u;
[[noreturn]] void FileFailure(const char* operation,const std::filesystem::path& path,int error){
    throw std::filesystem::filesystem_error(operation,path,std::error_code(error,std::system_category()));
}
class NativeFiles final:public SaveFileOps {
public:
    bool WriteNewAndFlush(const std::filesystem::path& path,std::string_view text) override{
        if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
#ifdef _WIN32
        const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE){
            const auto error=GetLastError();
            if(error==ERROR_FILE_EXISTS||error==ERROR_ALREADY_EXISTS)return false;
            FileFailure("create temporary save",path,static_cast<int>(error));
        }
        bool closed=false;
        try{
            std::size_t offset=0;
            while(offset<text.size()){
                DWORD written=0;
                const DWORD count=static_cast<DWORD>(std::min<std::size_t>(text.size()-offset,
                    std::numeric_limits<DWORD>::max()));
                if(!WriteFile(file,text.data()+offset,count,&written,nullptr))
                    FileFailure("write temporary save",path,static_cast<int>(GetLastError()));
                if(!written)throw std::runtime_error("Temporary save write made no progress");
                offset+=written;
            }
            if(!FlushFileBuffers(file))FileFailure("flush temporary save",path,static_cast<int>(GetLastError()));
            const auto result=CloseHandle(file);closed=true;
            if(!result)FileFailure("close temporary save",path,static_cast<int>(GetLastError()));
        }catch(...){if(!closed)CloseHandle(file);Remove(path);throw;}
#else
        const int file=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if(file<0){if(errno==EEXIST)return false;FileFailure("create temporary save",path,errno);}
        bool closed=false;
        try{
            std::size_t offset=0;
            while(offset<text.size()){
                const auto written=::write(file,text.data()+offset,text.size()-offset);
                if(written<0){if(errno==EINTR)continue;FileFailure("write temporary save",path,errno);}
                if(!written)throw std::runtime_error("Temporary save write made no progress");
                offset+=static_cast<std::size_t>(written);
            }
            int result;do{result=::fsync(file);}while(result<0&&errno==EINTR);
            if(result<0)FileFailure("flush temporary save",path,errno);
            result=::close(file);closed=true;
            if(result<0)FileFailure("close temporary save",path,errno);
        }catch(...){if(!closed)::close(file);Remove(path);throw;}
#endif
        return true;
    }
    std::string Read(const std::filesystem::path& path) override{
        const auto size=std::filesystem::file_size(path);
        if(size>kMaximumSaveBytes)throw std::runtime_error("Native save exceeds its 64 MiB limit");
        std::ifstream input(path,std::ios::binary);
        if(!input)FileFailure("open Native save",path,errno);
        std::string text(static_cast<std::size_t>(size),'\0');
        input.read(text.data(),static_cast<std::streamsize>(text.size()));
        if(!input||input.peek()!=std::char_traits<char>::eof())
            throw std::runtime_error("Native save changed size or could not be read completely");
        return text;
    }
    void Replace(const std::filesystem::path& temporary,const std::filesystem::path& target) override{
        if(temporary.parent_path()!=target.parent_path())
            throw std::runtime_error("Native save temporary must share its target directory");
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            FileFailure("replace Native save",target,static_cast<int>(GetLastError()));
#else
        if(::rename(temporary.c_str(),target.c_str())<0)FileFailure("replace Native save",target,errno);
        const auto parent=target.parent_path().empty()?std::filesystem::path("."):target.parent_path();
        const int directory=::open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        if(directory<0)FileFailure("open committed save directory",parent,errno);
        int result;do{result=::fsync(directory);}while(result<0&&errno==EINTR);
        const int error=errno;::close(directory);
        if(result<0)FileFailure("flush committed save directory",parent,error);
#endif
    }
    void Remove(const std::filesystem::path& path) noexcept override{
        std::error_code ignored;std::filesystem::remove(path,ignored);
    }
};
std::filesystem::path NextTemporary(const std::filesystem::path& target){
    static std::atomic<std::uint64_t> sequence{0};
#ifdef _WIN32
    const auto process=GetCurrentProcessId();
#else
    const auto process=::getpid();
#endif
    auto path=target;
    path+=std::filesystem::path(".tmp."+std::to_string(process)+"."+
        std::to_string(sequence.fetch_add(1,std::memory_order_relaxed))).native();
    return path;
}
}
std::shared_ptr<SaveFileOps> NativeSaveFileOps(){
    static auto files=std::make_shared<NativeFiles>();return files;
}
void WriteValidatedSaveFile(const std::filesystem::path& target,std::string_view text,
    const std::function<void(std::string_view)>& validate,const std::shared_ptr<SaveFileOps>& files){
    if(!files||!validate)throw std::runtime_error("Native save writer requires files and validation");
    if(text.size()>kMaximumSaveBytes)throw std::runtime_error("Native save exceeds its 64 MiB limit");
    for(unsigned attempt=0;attempt<128;attempt++){
        const auto temporary=NextTemporary(target);
        if(!files->WriteNewAndFlush(temporary,text))continue;
        try{
            const auto actual=files->Read(temporary);validate(actual);
            // Catch partial/corrupted writes even if the truncated data would
            // independently be a valid older or smaller save.
            if(actual!=text)throw std::runtime_error("Temporary save differs from the requested checkpoint");
            files->Replace(temporary,target);
        }catch(...){files->Remove(temporary);throw;}
        return;
    }
    throw std::runtime_error("Could not allocate a unique Native save temporary");
}
}
