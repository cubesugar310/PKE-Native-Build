#include "pke/AssetLocator.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace pke {
namespace {
std::filesystem::path rootify(const std::filesystem::path& exe, const std::filesystem::path& p){ return (p.is_absolute()?p:exe/p).lexically_normal(); }
std::filesystem::path requirePath(const std::filesystem::path& p,const char* what){ if(!std::filesystem::exists(p)) throw std::runtime_error(std::string("Missing ")+what+": "+p.string()); return p.lexically_normal(); }
}
AssetLocator::AssetLocator(const Config& c,std::filesystem::path exe):songsRoot_(rootify(exe,c.songsRoot)),charactersRoot_(rootify(exe,c.charactersRoot)){}
std::filesystem::path AssetLocator::ResolveSongFolder(std::string_view id) const { return requirePath(songsRoot_/std::string(id),"song folder"); }
std::filesystem::path AssetLocator::ResolveInst(const std::filesystem::path& f) const { return requirePath(f/"Inst.ogg","Inst.ogg"); }
std::optional<std::filesystem::path> AssetLocator::ResolveVoices(const std::filesystem::path& f) const { auto p=f/"Voices.ogg"; if(std::filesystem::exists(p)) return p.lexically_normal(); return std::nullopt; }
std::filesystem::path AssetLocator::ResolveChartFile(const std::filesystem::path& f,std::string_view difficulty) const {
    const std::string folderName=f.filename().string(); const std::string d(difficulty);
    std::vector<std::filesystem::path> candidates;
    if(!d.empty()){ candidates.push_back(f/(folderName+"-"+d+".json")); candidates.push_back(f/(d+".json")); }
    candidates.push_back(f/(folderName+".json"));
    for(const auto& p:candidates) if(std::filesystem::exists(p)) return p.lexically_normal();
    throw std::runtime_error("Missing chart file; tried first expected path: "+candidates.front().string());
}
CharacterAssetPaths AssetLocator::ResolveCharacter(std::string_view id) const {
    const std::string s(id);
    std::filesystem::path base=charactersRoot_/s;
    std::filesystem::path json=charactersRoot_/(s+".json");
    if(std::filesystem::exists(base/(s+".json"))) { json=base/(s+".json"); base=base; }
    else base=charactersRoot_;
    requirePath(json,"character json");
    CharacterAssetPaths out; out.json=json.lexically_normal();
    for(auto ext:{".xml"}) { auto p=base/(s+ext); if(std::filesystem::exists(p)){ out.xml=p.lexically_normal(); break; } }
    for(auto ext:{".png",".webp"}) { auto p=base/(s+ext); if(std::filesystem::exists(p)){ out.image=p.lexically_normal(); break; } }
    return out;
}
}
