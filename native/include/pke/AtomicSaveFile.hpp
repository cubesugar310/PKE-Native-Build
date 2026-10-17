#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace pke {
// Filesystem boundary for durable writes and controlled failure tests. On a
// failed new-file write the implementation removes its own partial file. A
// false return means the name already exists and must remain untouched.
class SaveFileOps {
public:
    virtual ~SaveFileOps()=default;
    virtual bool WriteNewAndFlush(const std::filesystem::path& path,std::string_view text)=0;
    virtual std::string Read(const std::filesystem::path& path)=0;
    virtual void Replace(const std::filesystem::path& temporary,const std::filesystem::path& target)=0;
    virtual void Remove(const std::filesystem::path& path) noexcept=0;
};
std::shared_ptr<SaveFileOps> NativeSaveFileOps();
// Writes beside the durable target, closes/flushes, reads and validates the
// actual temporary bytes, then renames over the target. Never copies over it.
void WriteValidatedSaveFile(const std::filesystem::path& target,std::string_view text,
    const std::function<void(std::string_view)>& validate,
    const std::shared_ptr<SaveFileOps>& files=NativeSaveFileOps());
}
