#include "pke/CharacterLoader.hpp"
#include "pke/CharacterPresentation.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

#ifdef PKE_NATIVE_USE_NLOHMANN
#include <nlohmann/json.hpp>
#include <tinyxml2.h>
#else
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#endif

namespace pke {
namespace {
int suffixNumber(const std::string& name,const std::string& prefix){
    if(name.rfind(prefix,0)!=0) return -1;
    auto tail=name.substr(prefix.size()); size_t i=0; while(i<tail.size() && !std::isdigit(static_cast<unsigned char>(tail[i]))) ++i;
    if(i==tail.size()) return 0;
    int v=0;
    for(;i<tail.size() && std::isdigit(static_cast<unsigned char>(tail[i]));++i) v=v*10+(tail[i]-'0');
    return v;
}

struct ResolvedCharacterFiles { std::filesystem::path xml; std::filesystem::path image; };
ResolvedCharacterFiles resolveCharacterFiles(const CharacterAssetPaths& paths,const std::string& imageField){
    ResolvedCharacterFiles r{paths.xml,paths.image};
    if(imageField.empty()) return r;
    auto base=std::filesystem::path(imageField).filename();
    if(base.has_extension()) base.replace_extension();
    const auto parent=paths.json.parent_path();
    if(r.xml.empty()){
        auto candidate=parent/(base.string()+".xml");
        if(std::filesystem::exists(candidate)) r.xml=candidate;
    }
    if(r.image.empty()){
        for(const char* ext:{".png",".webp"}){
            auto candidate=parent/(base.string()+ext);
            if(std::filesystem::exists(candidate)){ r.image=candidate; break; }
        }
    }
    return r;
}
void bindFrames(CharacterDefinition& out){
    for(auto& [key,a]:out.animations){
        std::vector<std::pair<int,size_t>> matches;
        for(size_t i=0;i<out.atlas.size();++i){ int n=suffixNumber(out.atlas[i].name,a.prefix); if(n>=0) matches.push_back({n,i}); }
        std::stable_sort(matches.begin(),matches.end(),[](auto x,auto y){ return x.first<y.first; });
        if(a.indices.empty()) for(auto [n,i]:matches) a.frameIndices.push_back(i);
        else for(int wanted:a.indices){ auto it=std::find_if(matches.begin(),matches.end(),[&](auto p){return p.first==wanted;}); if(it!=matches.end()) a.frameIndices.push_back(it->second); }
        if(a.frameIndices.empty()) out.warnings.push_back("Animation '"+a.anim+"' matched no atlas frames for prefix '"+a.prefix+"'");
    }
    auto idle=out.animations.find("idle"); if(idle!=out.animations.end() && !idle->second.frameIndices.empty()) for(auto& [k,a]:out.animations) if(a.frameIndices.empty() && k!="idle") a.frameIndices=idle->second.frameIndices;
}
}

FramePlacement ComputeFramePlacement(const AtlasFrame& f, SDL_Point authoredOffset, double scale, bool flipX){
    const int logicalW=f.rotated?f.source.h:f.source.w, logicalH=f.rotated?f.source.w:f.source.h;
    const double fullW=f.frameWidth>0?f.frameWidth:logicalW;
    const double left=-double(f.frameX), top=-double(f.frameY);
    const double x=flipX ? double(fullW)-left-double(logicalW) : left;
    return {f.source, authoredOffset.x+x*scale, authoredOffset.y+top*scale, logicalW*scale, logicalH*scale, f.rotated, flipX};
}

CharacterDefinition LoadCharacter(const CharacterAssetPaths& paths){
    CharacterDefinition out;
    try {
#ifdef PKE_NATIVE_USE_NLOHMANN
        std::ifstream jin(paths.json); if(!jin) throw std::runtime_error("cannot open character json"); nlohmann::json j; jin>>j;
        out.scale=j.value("scale",1.0); out.flipX=j.value("flip_x",false); out.noAntialiasing=j.value("no_antialiasing",false); out.singDuration=j.value("sing_duration",4.0);
        out.opponentFlipX=j.contains("flip_x")&&j["flip_x"].is_boolean()?j["flip_x"].get<bool>():j.contains("flipX")&&j["flipX"].is_boolean()?j["flipX"].get<bool>():false;
        if(j.contains("position")&&j["position"].is_array()&&j["position"].size()>=2) out.position={j["position"][0].get<int>(),j["position"][1].get<int>()};
        std::string image=j.value("image",""); auto resolved=resolveCharacterFiles(paths,image); out.imagePath=resolved.image;
        if(j.contains("animations")) for(const auto& a:j["animations"]){ AnimationDef d; const bool hasAnim=a.contains("anim"); d.anim=hasAnim?a.value("anim",""):a.value("name",""); d.prefix=hasAnim?a.value("name",a.value("prefix","")):a.value("prefix",d.anim); d.fps=a.value("fps",24.0); d.loop=a.value("loop",false); if(a.contains("offsets")&&a["offsets"].size()>=2)d.offset={a["offsets"][0].get<double>(),a["offsets"][1].get<double>()}; if(a.contains("indices")) for(auto& v:a["indices"]) d.indices.push_back(v.get<int>()); if(!out.animations.contains(d.anim))out.animationOrder.push_back(d.anim);out.animations[d.anim]=std::move(d); }
        if(resolved.xml.empty()) throw std::runtime_error("cannot locate character xml from image field '"+image+"'");
        tinyxml2::XMLDocument doc;
        std::ifstream xmlInput(resolved.xml,std::ios::binary);
        if(!xmlInput)throw std::runtime_error("cannot open character xml");
        std::ostringstream xmlBytes;xmlBytes<<xmlInput.rdbuf();const auto xmlText=xmlBytes.str();
        if(doc.Parse(xmlText.data(),xmlText.size())!=tinyxml2::XML_SUCCESS) throw std::runtime_error("cannot parse character xml");
        auto* root=doc.FirstChildElement("TextureAtlas");
        if(!root) throw std::runtime_error("TextureAtlas missing");
        for(auto* e=root->FirstChildElement("SubTexture");e;e=e->NextSiblingElement("SubTexture")){ AtlasFrame f; f.name=e->Attribute("name")?e->Attribute("name"):""; e->QueryIntAttribute("x",&f.source.x); e->QueryIntAttribute("y",&f.source.y); e->QueryIntAttribute("width",&f.source.w); e->QueryIntAttribute("height",&f.source.h); e->QueryDoubleAttribute("frameX",&f.frameX); e->QueryDoubleAttribute("frameY",&f.frameY); e->QueryDoubleAttribute("frameWidth",&f.frameWidth); e->QueryDoubleAttribute("frameHeight",&f.frameHeight); const char* r=e->Attribute("rotated"); f.rotated=r && std::string(r)=="true"; out.atlas.push_back(std::move(f)); }
#else
        using boost::property_tree::ptree; ptree j; boost::property_tree::read_json(paths.json.string(),j);
        out.scale=j.get<double>("scale",1.0); out.flipX=j.get<bool>("flip_x",false); out.noAntialiasing=j.get<bool>("no_antialiasing",false); out.singDuration=j.get<double>("sing_duration",4.0);
        out.opponentFlipX=j.get_optional<bool>("flip_x").value_or(j.get<bool>("flipX",false));
        if(auto p=j.get_child_optional("position")){ auto it=p->begin(); if(it!=p->end()){out.position.x=it->second.get_value<int>(); ++it;} if(it!=p->end()) out.position.y=it->second.get_value<int>(); }
        std::string image=j.get<std::string>("image",""); auto resolved=resolveCharacterFiles(paths,image); out.imagePath=resolved.image;
        if(auto anims=j.get_child_optional("animations")) for(auto& ap:*anims){ const auto& a=ap.second; AnimationDef d; if(auto anim=a.get_optional<std::string>("anim")){ d.anim=*anim; d.prefix=a.get<std::string>("name",a.get<std::string>("prefix","")); } else { d.anim=a.get<std::string>("name",""); d.prefix=a.get<std::string>("prefix",d.anim); } d.fps=a.get<double>("fps",24.0); d.loop=a.get<bool>("loop",false); if(auto o=a.get_child_optional("offsets")){auto it=o->begin(); if(it!=o->end()){d.offset.x=it->second.get_value<double>();++it;} if(it!=o->end())d.offset.y=it->second.get_value<double>();} if(auto inds=a.get_child_optional("indices"))for(auto& v:*inds)d.indices.push_back(v.second.get_value<int>()); if(!out.animations.contains(d.anim))out.animationOrder.push_back(d.anim);out.animations[d.anim]=std::move(d); }
        if(resolved.xml.empty()) throw std::runtime_error("cannot locate character xml from image field '"+image+"'");
        ptree x;
        boost::property_tree::read_xml(resolved.xml.string(),x,boost::property_tree::xml_parser::trim_whitespace);
        auto root=x.get_child_optional("TextureAtlas");
        if(!root) throw std::runtime_error("TextureAtlas missing");
        for(auto& child:*root) if(child.first=="SubTexture"){ auto& a=child.second.get_child("<xmlattr>"); AtlasFrame f; f.name=a.get<std::string>("name",""); f.source.x=a.get<int>("x",0); f.source.y=a.get<int>("y",0); f.source.w=a.get<int>("width",0); f.source.h=a.get<int>("height",0); f.frameX=a.get<double>("frameX",0); f.frameY=a.get<double>("frameY",0); f.frameWidth=a.get<double>("frameWidth",f.source.w); f.frameHeight=a.get<double>("frameHeight",f.source.h); f.rotated=a.get<std::string>("rotated","false")=="true"; out.atlas.push_back(std::move(f)); }
#endif
        bindFrames(out); PrepareCharacterGeometry(out); return out;
    } catch(const std::exception& e){ throw std::runtime_error("Failed to load character "+paths.json.string()+": "+e.what()); }
}
}
