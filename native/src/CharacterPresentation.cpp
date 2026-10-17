#include "pke/CharacterPresentation.hpp"
#include "pke/QuickJsRuntime.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#ifdef PKE_NATIVE_USE_NLOHMANN
#include <nlohmann/json.hpp>
#else
#include <boost/json.hpp>
#endif

namespace pke {
namespace {
double Median(std::vector<double> values){
    std::erase_if(values,[](double v){return !std::isfinite(v)||v<=0;});
    if(values.empty())return 1;std::sort(values.begin(),values.end());const auto mid=values.size()/2;
    return values.size()%2?values[mid]:(values[mid-1]+values[mid])/2;
}
bool Near(double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=.01;}
bool SameCanvas(const AtlasFrame& a,const AtlasFrame& b){return Near(a.frameWidth,b.frameWidth)&&Near(a.frameHeight,b.frameHeight);}
void Stabilize(CharacterDefinition& c,const AnimationDef& animation){
    const auto& indices=animation.frameIndices;
    for(std::size_t i=1;i+1<indices.size();++i){
        if(indices[i-1]>=c.atlas.size()||indices[i]>=c.atlas.size()||indices[i+1]>=c.atlas.size())continue;
        const auto& previous=c.atlas[indices[i-1]];auto& current=c.atlas[indices[i]];const auto& next=c.atlas[indices[i+1]];
        if(previous.source.x!=current.source.x||previous.source.y!=current.source.y||previous.source.w!=current.source.w||previous.source.h!=current.source.h||!SameCanvas(previous,current)||!SameCanvas(previous,next))continue;
        const auto fix=[](double before,double& value,double after,double size){
            if(!std::isfinite(before)||!std::isfinite(value)||!std::isfinite(after))return;
            const double logical=std::max(1.0,size),jump=std::max(24.0,logical*.08),tolerance=std::max(8.0,logical*.03);
            if(std::abs(value-before)>=jump&&std::abs(value-after)>=jump&&std::abs(after-before)<=tolerance)value=(before+after)/2;
        };
        fix(previous.frameX,current.frameX,next.frameX,current.frameWidth);fix(previous.frameY,current.frameY,next.frameY,current.frameHeight);
    }
}
std::string Read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Character presentation script missing: "+path.string());std::ostringstream s;s<<f.rdbuf();return s.str();}
std::string Require(QuickJsResult result){if(!result.ok)throw std::runtime_error(result.error);return std::move(result.json);}
#ifdef PKE_NATIVE_USE_NLOHMANN
using Json=nlohmann::json;
const Json& At(const Json& j,const char* k){return j.at(k);}
double Number(const Json& j){return j.get<double>();}
bool Null(const Json& j){return j.is_null();}
bool Bool(const Json& j){return j.get<bool>();}
Json Parse(const std::string& s){return Json::parse(s);}
std::string JsonString(std::string_view s){return Json(std::string(s)).dump();}
template<class F>void Each(const Json& j,F visit){for(auto it=j.begin();it!=j.end();++it)visit(it.key(),it.value());}
#else
using Json=boost::json::value;
const Json& At(const Json& j,const char* k){return j.as_object().at(k);}
double Number(const Json& j){return j.is_double()?j.as_double():j.is_int64()?double(j.as_int64()):double(j.as_uint64());}
bool Null(const Json& j){return j.is_null();}
bool Bool(const Json& j){return j.as_bool();}
Json Parse(const std::string& s){return boost::json::parse(s);}
std::string JsonString(std::string_view s){return boost::json::serialize(boost::json::value(std::string(s)));}
template<class F>void Each(const Json& j,F visit){for(const auto& p:j.as_object())visit(std::string(p.key()),p.value());}
#endif
}
void PrepareCharacterGeometry(CharacterDefinition& c){
    if(c.animationOrder.empty())for(const auto& [name,a]:c.animations)c.animationOrder.push_back(name);
    // BF stabilizes XML name groups before JSON indices; Dad skips that pass.
    auto originalAtlas=c.atlas;
    std::map<std::string,AnimationDef,std::less<>> xmlGroups;
    for(std::size_t i=0;i<c.atlas.size();++i){const auto& name=c.atlas[i].name;auto end=name.size();
        while(end&&name[end-1]>='0'&&name[end-1]<='9')--end;
        xmlGroups[name.substr(0,end)].frameIndices.push_back(i);
    }
    for(auto& [name,group]:xmlGroups){
        const auto suffix=[&](std::size_t i){const auto tail=c.atlas[i].name.substr(name.size());return tail.empty()?0:std::strtod(tail.c_str(),nullptr);};
        std::stable_sort(group.frameIndices.begin(),group.frameIndices.end(),[&](auto a,auto b){return suffix(a)<suffix(b);});
        Stabilize(c,group);
    }
    for(const auto& name:c.animationOrder)if(const auto it=c.animations.find(name);it!=c.animations.end())Stabilize(c,it->second);
    c.playerTrimOffsets.clear();c.playerTrimOffsets.reserve(c.atlas.size());
    for(const auto& frame:c.atlas)c.playerTrimOffsets.push_back({frame.frameX,frame.frameY});
    c.atlas=std::move(originalAtlas);
    for(const auto& name:c.animationOrder)if(const auto it=c.animations.find(name);it!=c.animations.end())Stabilize(c,it->second);
    for(auto& [name,a]:c.animations){std::vector<double> widths,heights;
        for(const auto i:a.frameIndices)if(i<c.atlas.size()){const auto& f=c.atlas[i];
            const double width=f.frameWidth?f.frameWidth:(f.rotated?f.source.h:f.source.w),height=f.frameHeight?f.frameHeight:(f.rotated?f.source.w:f.source.h);
            widths.push_back(width?width:1);heights.push_back(height?height:1);}
        a.anchor={std::max(1.0,Median(std::move(widths))),std::max(1.0,Median(std::move(heights)))};
    }
    const auto idle=c.animations.find("idle");c.idleAnchor=idle!=c.animations.end()?idle->second.anchor:AnimationAnchor{};c.geometryPrepared=true;
}
FramePlacement ComputePracticeCharacterPlacement(const CharacterDefinition& c,AnimationFrameView frame,CharacterRole role,bool perspective,const CharacterTransform& profile){
    if(frame.atlasFrameIndex>=c.atlas.size())return {};
    const auto& f=c.atlas[frame.atlasFrameIndex];const auto found=c.animations.find(std::string(frame.animationName));
    const auto offset=found!=c.animations.end()?found->second.offset:CharacterPoint{};
    auto anchor=c.idleAnchor;
    if(!c.geometryPrepared){anchor={std::max(1.0,f.frameWidth?f.frameWidth:double(f.rotated?f.source.h:f.source.w)),std::max(1.0,f.frameHeight?f.frameHeight:double(f.rotated?f.source.w:f.source.h))};}
    else if(!c.animations.contains("idle")&&found!=c.animations.end())anchor=found->second.anchor;
    const auto pose=profile.poses.find(frame.animationName);const auto adjust=pose==profile.poses.end()?CharacterPoint{}:pose->second;
    const bool player=role==CharacterRole::Player;
    const auto trim=player&&frame.atlasFrameIndex<c.playerTrimOffsets.size()?c.playerTrimOffsets[frame.atlasFrameIndex]:CharacterPoint{f.frameX,f.frameY};
    const double authored=player?std::max(.05,c.scale==0?1:c.scale):(std::isfinite(c.scale)&&c.scale>0?c.scale:1);
    const double scale=std::clamp(1.58*authored*profile.scale,player?.05:.08,player?12.0:8.0);
    double x=(player?720+448*.82:16+680*.39)+profile.offsetX+(player?adjust.x:0);
    const double y=(player?696:644)+profile.offsetY+(player?adjust.y:0);
    if(perspective)x=1280-x;
    const double w=(f.rotated?f.source.h:f.source.w)*scale,h=(f.rotated?f.source.w:f.source.h)*scale;
    double dx=x-anchor.width*scale/2+(-trim.x-offset.x)*scale+(player?0:adjust.x);
    const double dy=y-anchor.height*scale+(-trim.y-offset.y)*scale+(player?0:adjust.y);
    const bool flip=profile.flipXOverride.value_or(player?!c.flipX:c.opponentFlipX.value_or(c.flipX))!=perspective;
    if(flip)dx=2*x-dx-w;
    return {f.source,dx,dy,w,h,f.rotated,flip};
}
std::pair<CharacterTransform,CharacterTransform> LoadCharacterTransforms(const std::filesystem::path& compat,std::string_view saved,const CharacterAsset* bf,const CharacterAsset* dad,std::string_view history){
    QuickJsRuntime vm;Require(vm.Evaluate(Read(compat/"character-presentation.js"),"character-presentation.js"));
    const auto assetInput=[&](const CharacterAsset* asset){
        const std::string id=asset?asset->id:"",skin=asset?(asset->sourceSkinId.empty()?asset->id:asset->sourceSkinId):"";
        const std::string descriptor=asset&&!asset->presentationDescriptor.empty()?asset->presentationDescriptor:
            "{\"type\":\"sparrow\",\"characterId\":"+JsonString(id)+'}';
        return "{\"id\":"+JsonString(id)+",\"skinId\":"+JsonString(skin)+",\"definition\":"+descriptor+'}';
    };
    const auto input="{\"layouts\":"+std::string(saved)+",\"identityHistory\":"+std::string(history)+",\"player\":"+assetInput(bf)+",\"opponent\":"+assetInput(dad)+'}';
    const auto profiles=Parse(Require(vm.CallJson("pkeCharacterProfiles",input)));
    const auto load=[&](const Json& data){CharacterTransform p;
        p.scale=Number(At(data,"scale"));p.offsetX=Number(At(data,"offsetX"));p.offsetY=Number(At(data,"offsetY"));
        if(!Null(At(data,"flipXOverride")))p.flipXOverride=Bool(At(data,"flipXOverride"));
        Each(At(data,"poses"),[&](const auto& name,const auto& point){p.poses.emplace(name,CharacterPoint{Number(At(point,"x")),Number(At(point,"y"))});});return p;
    };
    auto player=load(At(profiles,"player"));auto opponent=load(At(profiles,"opponent"));return {std::move(player),std::move(opponent)};
}
}
