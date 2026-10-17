#include "pke/SaveService.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>
#ifdef PKE_NATIVE_USE_NLOHMANN
#include <nlohmann/json.hpp>
#else
#include <boost/json.hpp>
#endif

namespace pke {
namespace {
constexpr double kSafeInteger=9007199254740991.0;
constexpr std::size_t kMaximumBytes=64u*1024u*1024u;
[[noreturn]] void Invalid(const char* message){throw std::runtime_error(message);}
#ifdef PKE_NATIVE_USE_NLOHMANN
using Json=nlohmann::json;
bool Object(const Json& value){return value.is_object();}
bool Array(const Json& value){return value.is_array();}
bool Has(const Json& value,std::string_view key){return value.contains(std::string(key));}
const Json& At(const Json& value,std::string_view key){return value.at(std::string(key));}
std::string_view String(const Json& value){
    if(!value.is_string())Invalid("Expected Native save string");
    return value.get_ref<const std::string&>();
}
double Number(const Json& value){
    if(!value.is_number())Invalid("Expected Native save number");
    return value.get<double>();
}
bool Boolean(const Json& value){return value.is_boolean();}
template<class F>void EachObject(const Json& value,F visit){
    for(auto it=value.begin();it!=value.end();++it)visit(std::string_view(it.key()),it.value());
}
template<class F>void EachArray(const Json& value,F visit){for(const auto& item:value)visit(item);}
std::size_t Size(const Json& value){return value.size();}
Json Parse(std::string_view text){return Json::parse(text);}
bool Null(const Json& v){return v.is_null();}
std::string Encode(const Json& v){return v.dump();}
#else
using Json=boost::json::value;
bool Object(const Json& value){return value.is_object();}
bool Array(const Json& value){return value.is_array();}
auto Key(std::string_view key){return boost::json::string_view(key.data(),key.size());}
bool Has(const Json& value,std::string_view key){return Object(value)&&value.as_object().contains(Key(key));}
const Json& At(const Json& value,std::string_view key){return value.as_object().at(Key(key));}
std::string_view String(const Json& value){
    if(!value.is_string())Invalid("Expected Native save string");
    const auto& text=value.as_string();return {text.data(),text.size()};
}
double Number(const Json& value){
    if(value.is_int64())return static_cast<double>(value.as_int64());
    if(value.is_uint64())return static_cast<double>(value.as_uint64());
    if(value.is_double())return value.as_double();
    Invalid("Expected Native save number");
}
bool Boolean(const Json& value){return value.is_bool();}
template<class F>void EachObject(const Json& value,F visit){
    for(const auto& pair:value.as_object())visit(std::string_view(pair.key().data(),pair.key().size()),pair.value());
}
template<class F>void EachArray(const Json& value,F visit){for(const auto& item:value.as_array())visit(item);}
std::size_t Size(const Json& value){return Array(value)?value.as_array().size():value.as_object().size();}
Json Parse(std::string_view text){return boost::json::parse(Key(text));}
bool Null(const Json& v){return v.is_null();}
std::string Encode(const Json& v){return boost::json::serialize(v);}
#endif

Json ParseBounded(std::string_view text){
    if(text.size()>kMaximumBytes)Invalid("Native save exceeds its 64 MiB limit");
    // Bound nesting before entering either parser, including on malformed
    // input. Quoted braces and escaped quotes do not count as structure.
    unsigned depth=0;bool quoted=false,escaped=false;
    for(const char c:text){
        if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;
        else if(c=='{'||c=='['){if(++depth>64)Invalid("Native save nesting is too deep");}
        else if(c=='}'||c==']'){if(!depth)Invalid("Unbalanced Native save");--depth;}
    }
    return Parse(text);
}
void RequireObject(const Json& value){if(!Object(value))Invalid("Expected Native save object");}
void RequireArray(const Json& value,std::size_t limit){
    if(!Array(value)||Size(value)>limit)Invalid("Invalid Native save array");
}
double Range(const Json& value,double minimum=0,double maximum=kSafeInteger){
    const auto number=Number(value);
    if(!std::isfinite(number)||number<minimum||number>maximum)Invalid("Native save number is outside its valid range");
    return number;
}
std::int64_t Integer(const Json& value,double minimum=0,double maximum=kSafeInteger){
    const auto number=Range(value,minimum,maximum);
    if(std::floor(number)!=number)Invalid("Native save requires an integer");
    return static_cast<std::int64_t>(number);
}
void Exact(const Json& value,int expected){if(Integer(value)!=expected)Invalid("Unsupported Native save version");}
bool PlayerId(std::string_view id){
    if(id=="YOU")return true;
    if(id.size()!=5)return false;
    unsigned number=0;for(char c:id){if(c<'0'||c>'9')return false;number=number*10+(c-'0');}
    return number>=1&&number<=14999;
}
void Id(const Json& value){if(!PlayerId(String(value)))Invalid("Invalid Native population player id");}
void SongRange(const Json& value){
    const auto text=String(value);
    if(text!="beginner"&&text!="standard"&&text!="all")Invalid("Invalid Native song range");
}
void Strings(const Json& value){
    RequireArray(value,15000);EachArray(value,[](const auto& item){String(item);});
}
const Json* Optional(const Json& value,std::string_view key){return Has(value,key)?&At(value,key):nullptr;}
std::string TextOr(const Json* value,std::string fallback={}){
    if(!value||Null(*value))return fallback;
    try{return std::string(String(*value));}catch(...){return Encode(*value);}
}
std::string Trim(std::string text){
    const auto first=text.find_first_not_of(" \t\n\r");if(first==std::string::npos)return {};
    return text.substr(first,text.find_last_not_of(" \t\n\r")-first+1);
}
std::uint64_t SafeOr(const Json* value,std::uint64_t fallback=0,std::uint64_t max=9007199254740991ULL){
    if(!value)return fallback;
    double number{};
    try{number=Number(*value);}catch(...){
        const auto text=Trim(TextOr(value));if(text.empty())number=0;
        else {char* end{};number=std::strtod(text.c_str(),&end);if(end==text.c_str()||*end)return fallback;}
    }
    if(!std::isfinite(number))return fallback;
    return static_cast<std::uint64_t>(std::clamp(std::floor(number),0.0,static_cast<double>(max)));
}
std::vector<std::string> IdList(const Json* value){
    std::vector<std::string> result;if(!value||!Array(*value))return result;
    EachArray(*value,[&](const auto& item){if(Null(item))return;auto id=Trim(TextOr(&item));
        if(!id.empty()&&std::find(result.begin(),result.end(),id)==result.end())result.push_back(std::move(id));});return result;
}
bool Year(std::string_view year){return year.size()==4&&std::all_of(year.begin(),year.end(),[](char c){return c>='0'&&c<='9';});}
std::map<std::string,HalloweenHistory> History(const Json* value){
    std::map<std::string,HalloweenHistory> result;if(!value||!Object(*value))return result;
    EachObject(*value,[&](auto year,const auto& entry){if(!Year(year)||!Object(entry))return;
        result.emplace(std::string(year),HalloweenHistory{SafeOr(Optional(entry,"candyEarned")),SafeOr(Optional(entry,"onlineCompleted")),
            SafeOr(Optional(entry,"onlineWins")),SafeOr(Optional(entry,"halloweenBossWins")),IdList(Optional(entry,"purchases"))});});return result;
}
template<class F>void PlayerMap(const Json& value,F visit){
    RequireObject(value);if(Size(value)>15000)Invalid("Native population map is too large");
    EachObject(value,[&](auto key,const auto& item){
        if(!PlayerId(key))Invalid("Invalid Native population map key");visit(item);
    });
}
void League(const Json& value){
    RequireObject(value);
    for(const auto* key:{"rp","wins","losses","draws","matches"})Integer(At(value,key));
    SongRange(At(value,"onlineSongRangeTier"));
}
bool Missing(const std::filesystem::filesystem_error& error){
    return error.code()==std::errc::no_such_file_or_directory;
}
void ProtectLegacyDestination(const std::filesystem::path& path,const std::shared_ptr<SaveFileOps>& files,
                              const std::function<void(std::string_view)>& validate){
    std::string text;
    try{text=files->Read(path);}
    catch(const std::filesystem::filesystem_error& e){if(Missing(e))return;throw;}
    // Existing bytes whose identity cannot be established safely are preserved.
    // A legacy export may validly exceed the Native format's nesting limit.
    const auto value=ParseBounded(text);
    const bool legacy=Object(value)&&Has(value,"format")&&String(At(value,"format"))=="pke-beat-battle-full-save";
    if(legacy)Invalid("Refusing to replace a legacy migration source");
    validate(text);
}
}

std::string NormalizeCharacterLayoutsJson(std::string_view text){const auto root=ParseBounded(text);RequireObject(root);return Encode(root);}
void ImportHalloweenState(HalloweenData& out,std::string_view text){
    if(text.empty())return;const auto value=ParseBounded(text);if(!Object(value))return;
    out.halloweenCandy=SafeOr(Optional(value,"candy"));out.purchasedSkinIds=IdList(Optional(value,"purchasedSkinIds"));
    out.unlockedTitleIds=IdList(Optional(value,"unlockedTitleIds"));out.claimedRewardIds=IdList(Optional(value,"claimedRewardIds"));
    out.halloweenHistory=History(Optional(value,"historyByYear"));
}
void ImportHalloweenConfig(HalloweenData& out,std::string_view text){
    if(text.empty())return;const auto value=ParseBounded(text);if(!Object(value))return;
    HalloweenDevConfig config;const auto* slots=Optional(value,"skinSlots");std::size_t index=0;
    if(slots&&Array(*slots))EachArray(*slots,[&](const auto& slot){if(index>=4)return;
        config.skinSlots[index++]={Trim(TextOr(Optional(slot,"skinId"))),static_cast<int>(SafeOr(Optional(slot,"price"),3000,999999))};});
    const auto* refs=Optional(value,"bossSongRefs");index=0;
    if(refs&&Array(*refs))EachArray(*refs,[&](const auto& ref){if(index<3)config.bossSongRefs[index++]=Trim(TextOr(&ref));});
    else config.bossSongRefs[0]=Trim(TextOr(Optional(value,"bossSongRef")));
    const auto mode=TextOr(Optional(value,"eventOverride"));if(mode=="force-on"||mode=="force-off")config.eventOverride=mode;
    out.halloweenDev=std::move(config);
}

static void ValidatePlayerDocument(const Json& root){
    RequireObject(root);
    if(String(At(root,"format"))!="pke-native-save")Invalid("Invalid Native player save format");
    Exact(At(root,"version"),1);
    for(const auto* key:{"importedAt","legacyExportedAt","legacyBackupFile","legacySourcePath",
                         "playerName","equippedTitle","selectedSkin"})String(At(root,key));
    const auto& player=At(root,"player");RequireObject(player);
    for(const auto* key:{"rp","wins","losses","draws","peakRp"})
        Integer(At(player,key),0,std::numeric_limits<int>::max());
    for(const auto* key:{"matches","accuracyMatches","recentAt"})Integer(At(player,key));
    Range(At(player,"accuracy"),0,100);Range(At(player,"accuracySum"));
    SongRange(At(player,"onlineSongRangeTier"));String(At(player,"lastOpponentId"));
    const auto& settings=At(root,"settings");RequireObject(settings);
    Range(At(settings,"masterVolume"),0,1);
    const auto scroll=String(At(settings,"scrollDirection")),judgement=String(At(settings,"judgementPosition"));
    if(scroll!="up"&&scroll!="down")Invalid("Invalid Native scroll direction");
    if(judgement!="center"&&judgement!="corner")Invalid("Invalid Native judgement position");
    if(Has(root,"characterLayouts"))RequireObject(At(root,"characterLayouts"));
    if(Has(root,"characterIdentityHistory"))RequireObject(At(root,"characterIdentityHistory"));
    const auto& halloween=At(root,"halloween");RequireObject(halloween);
    Integer(At(halloween,"candy"));
    Strings(At(halloween,"purchasedSkinIds"));Strings(At(halloween,"unlockedTitleIds"));Strings(At(root,"earnedTitleIds"));
    if(Has(halloween,"claimedRewardIds"))Strings(At(halloween,"claimedRewardIds"));
    if(Has(halloween,"historyByYear")){const auto& history=At(halloween,"historyByYear");RequireObject(history);
        EachObject(history,[](auto year,const auto& entry){if(!Year(year))Invalid("Invalid Halloween year");RequireObject(entry);
            for(const auto* key:{"candyEarned","onlineCompleted","onlineWins","halloweenBossWins"})Integer(At(entry,key));
            Strings(At(entry,"purchases"));});}
    if(Has(root,"halloweenDev")){const auto& dev=At(root,"halloweenDev");RequireObject(dev);
        const auto mode=String(At(dev,"eventOverride"));if(mode!="auto"&&mode!="force-on"&&mode!="force-off")Invalid("Invalid event override");
        const auto& slots=At(dev,"skinSlots");RequireArray(slots,4);if(Size(slots)!=4)Invalid("Expected four shop slots");
        EachArray(slots,[](const auto& slot){RequireObject(slot);String(At(slot,"skinId"));Integer(At(slot,"price"),0,999999);});
        const auto& refs=At(dev,"bossSongRefs");Strings(refs);if(Size(refs)!=3)Invalid("Expected three Boss references");
    }
}
void ValidateNativePlayerSave(std::string_view text){ValidatePlayerDocument(ParseBounded(text));}
NativeSaveSummary DecodeNativePlayerSave(std::string_view text){
    const auto root=ParseBounded(text);ValidatePlayerDocument(root);
    NativeSaveSummary out;out.valid=true;
    out.importedAt=String(At(root,"importedAt"));out.legacyExportedAt=String(At(root,"legacyExportedAt"));
    out.legacyBackupFile=String(At(root,"legacyBackupFile"));out.legacySourcePath=String(At(root,"legacySourcePath"));
    out.playerName=String(At(root,"playerName"));out.equippedTitle=String(At(root,"equippedTitle"));
    out.selectedSkin=String(At(root,"selectedSkin"));
    const auto& player=At(root,"player");
    out.progress.rp=static_cast<int>(Integer(At(player,"rp")));
    out.progress.wins=static_cast<int>(Integer(At(player,"wins")));
    out.progress.losses=static_cast<int>(Integer(At(player,"losses")));
    out.progress.draws=static_cast<int>(Integer(At(player,"draws")));
    out.progress.peakRp=static_cast<int>(Integer(At(player,"peakRp")));
    out.progress.matches=static_cast<std::uint64_t>(Integer(At(player,"matches")));
    out.progress.accuracyMatches=static_cast<std::uint64_t>(Integer(At(player,"accuracyMatches")));
    out.progress.recentAt=static_cast<std::uint64_t>(Integer(At(player,"recentAt")));
    out.progress.accuracy=Number(At(player,"accuracy"));out.progress.accuracySum=Number(At(player,"accuracySum"));
    out.progress.onlineSongRangeTier=String(At(player,"onlineSongRangeTier"));
    out.progress.lastOpponentId=String(At(player,"lastOpponentId"));
    const auto& settings=At(root,"settings");out.masterVolume=Number(At(settings,"masterVolume"));
    out.scrollDirection=String(At(settings,"scrollDirection"));out.judgementPosition=String(At(settings,"judgementPosition"));
    if(Has(root,"characterLayouts"))out.characterLayoutsJson=Encode(At(root,"characterLayouts"));
    if(Has(root,"characterIdentityHistory"))out.characterIdentityHistoryJson=Encode(At(root,"characterIdentityHistory"));
    const auto& halloween=At(root,"halloween");out.halloweenCandy=static_cast<std::uint64_t>(Integer(At(halloween,"candy")));
    const auto strings=[](const Json& array){
        std::vector<std::string> result;EachArray(array,[&](const auto& item){result.emplace_back(String(item));});return result;
    };
    out.purchasedSkinIds=strings(At(halloween,"purchasedSkinIds"));
    out.unlockedTitleIds=strings(At(halloween,"unlockedTitleIds"));out.earnedTitleIds=strings(At(root,"earnedTitleIds"));
    if(Has(halloween,"claimedRewardIds"))out.claimedRewardIds=strings(At(halloween,"claimedRewardIds"));
    out.halloweenHistory=History(Optional(halloween,"historyByYear"));
    if(Has(root,"halloweenDev"))ImportHalloweenConfig(out,Encode(At(root,"halloweenDev")));
    return out;
}

std::int64_t ValidateNativePopulationSave(std::string_view text){
    const auto root=ParseBounded(text);RequireObject(root);
    if(String(At(root,"schema"))!="pke-native-population-v1")Invalid("Invalid Native population save schema");
    Exact(At(root,"version"),1);
    const auto now=Integer(At(root,"now"));const auto boot=Integer(At(root,"bootAt"));
    if(boot>now)Invalid("Invalid Native population boot clock");
    const auto& records=At(root,"records");RequireObject(records);
    Exact(At(records,"version"),12);Exact(At(records,"seasonId"),2);
    String(At(records,"seasonLabel"));Integer(At(records,"seasonStartedAt"));String(At(records,"lastOpponentId"));
    if(!Boolean(At(records,"seasonResetNoticePending")))Invalid("Invalid Native season flag");
    PlayerMap(At(records,"h2h"),[](const auto& item){RequireObject(item);});
    PlayerMap(At(records,"skinById"),[](const auto& item){String(item);});
    PlayerMap(At(records,"skinMoodById"),[](const auto& item){
        RequireObject(item);
        for(const auto* key:{"lossStreak","turn","nextNaturalAt","cooldownUntil"})if(Has(item,key))Integer(At(item,key));
        if(Has(item,"pendingChange"))String(At(item,"pendingChange"));
    });
    PlayerMap(At(records,"developmentById"),[](const auto& item){Integer(item);});
    PlayerMap(At(records,"leagueById"),League);
    const auto& titles=At(records,"titleState");RequireObject(titles);
    PlayerMap(At(titles,"peakRpById"),[](const auto& item){Integer(item);});
    PlayerMap(At(titles,"equippedById"),[](const auto& item){String(item);});
    PlayerMap(At(titles,"earnedById"),Strings);
    const auto& player=At(records,"player");RequireObject(player);
    Range(At(player,"accuracy"),0,100);Range(At(player,"accuracySum"));Integer(At(player,"accuracyMatches"));
    RequireArray(At(player,"recentMatches"),150000);SongRange(At(player,"onlineSongRangeTier"));
    const auto& background=At(records,"background");RequireObject(background);
    const auto tick=Integer(At(background,"lastTick"));
    if(tick>now||now-tick>=5000)Invalid("Invalid Native population checkpoint clock");
    Integer(At(background,"serial"));Integer(At(background,"elasticLiveRoomCap"),0,15000);
    Integer(At(background,"spectatorScanCursor"));
    for(const auto* key:{"liveMatches","bossMatches","privateMatches"}){
        const auto& rooms=At(background,key);RequireArray(rooms,15000);
        EachArray(rooms,[&](const auto& room){
            RequireObject(room);
            for(const auto* id:{"aId","bId"})if(std::string_view(key)=="liveMatches"||Has(room,id))Id(At(room,id));
            if(std::string_view(key)=="liveMatches"){
                if(String(At(room,"id")).empty()||String(At(room,"aId"))==String(At(room,"bId")))
                    Invalid("Invalid Native LIVE room identity");
                const auto started=Range(At(room,"startedAt"),-kSafeInteger);
                const auto ends=Range(At(room,"endsAt"),-kSafeInteger);
                if(started>now||ends<=started)Invalid("Invalid Native LIVE room clock");
            }else{
                if(Has(room,"id"))String(At(room,"id"));
                for(const auto* time:{"startedAt","endsAt"})if(Has(room,time))Range(At(room,time),-kSafeInteger);
            }
        });
    }
    for(const auto* key:{"practiceById","spectatorsById"})
        PlayerMap(At(background,key),[](const auto& item){RequireObject(item);});
    const auto& history=At(root,"history");RequireArray(history,150000);
    EachArray(history,[](const auto& match){
        RequireObject(match);String(At(match,"matchId"));Id(At(match,"aId"));Id(At(match,"bId"));
        Integer(At(match,"endedAt"));
        for(const auto* key:{"scoreA","scoreB"})Integer(At(match,key),0,100000);
        for(const auto* key:{"accuracyA","accuracyB"})Range(At(match,key),0,100);
    });
    return now;
}

SaveService::SaveService(std::filesystem::path root,std::shared_ptr<SaveFileOps> files)
    :root_(std::move(root)),files_(std::move(files)){
    if(!files_)throw std::invalid_argument("Native SaveService requires file operations");
}
PlayerSaveLoad SaveService::LoadPlayer() const{
    std::string text;
    try{text=files_->Read(root_/"native-save-v1.json");}
    catch(const std::filesystem::filesystem_error& e){return {Missing(e)?SaveLoadStatus::Missing:SaveLoadStatus::ReadFailed,{},e.what()};}
    catch(const std::exception& e){return {SaveLoadStatus::ReadFailed,{},e.what()};}
    try{
        auto player=DecodeNativePlayerSave(text);
        if(!player.valid)Invalid("Native player save could not be normalized");
        const auto root=ParseBounded(text);const auto& event=At(root,"halloween");
        if(!Has(root,"halloweenDev")||!Has(event,"historyByYear")||!Has(event,"claimedRewardIds")||!Has(root,"characterLayouts")||!Has(root,"characterIdentityHistory")){
            // Enrich only fields that the earlier Native summary did not store.
            // The preserved full backup never replaces current Candy/progress.
            const auto legacy=ReadLegacyFullSave(root_/"legacy-full-save-v1.json");
            if(legacy.valid){if(!Has(root,"halloweenDev"))player.halloweenDev=legacy.halloweenDev;
                if(!Has(event,"historyByYear"))player.halloweenHistory=legacy.halloweenHistory;
                if(!Has(event,"claimedRewardIds"))player.claimedRewardIds=legacy.claimedRewardIds;
                if(!Has(root,"characterLayouts"))player.characterLayoutsJson=legacy.characterLayoutsJson;
                if(!Has(root,"characterIdentityHistory"))player.characterIdentityHistoryJson=legacy.characterIdentityHistoryJson;}
        }
        return {SaveLoadStatus::Loaded,std::move(player),{}};
    }catch(const std::exception& e){return {SaveLoadStatus::Invalid,{},e.what()};}
}
PopulationSaveLoad SaveService::LoadPopulation() const{
    std::string text;
    try{text=files_->Read(root_/"population-state-v1.json");}
    catch(const std::filesystem::filesystem_error& e){return {Missing(e)?SaveLoadStatus::Missing:SaveLoadStatus::ReadFailed,{},e.what()};}
    catch(const std::exception& e){return {SaveLoadStatus::ReadFailed,{},e.what()};}
    try{
        const auto now=ValidateNativePopulationSave(text);
        return {SaveLoadStatus::Loaded,{true,std::move(text),{},now},{}};
    }catch(const std::exception& e){return {SaveLoadStatus::Invalid,{},e.what()};}
}
SaveResult SaveService::SavePlayer(const NativeSaveSummary& player) const{
    try{
        ProtectLegacyDestination(root_/"native-save-v1.json",files_,ValidateNativePlayerSave);
        WriteValidatedSaveFile(root_/"native-save-v1.json",SerializeNativeSaveSummary(player),ValidateNativePlayerSave,files_);
        return {true,{}};
    }catch(const std::exception& e){return {false,e.what()};}
}
SaveResult SaveService::SavePopulation(const PopulationCheckpoint& checkpoint) const{
    if(!checkpoint.ok)return {false,"Cannot save a failed population generation"};
    try{
        ProtectLegacyDestination(root_/"population-state-v1.json",files_,[](std::string_view text){ValidateNativePopulationSave(text);});
        const auto validate=[&](std::string_view text){
            if(ValidateNativePopulationSave(text)!=checkpoint.nowMs)Invalid("Population checkpoint clock does not match its generation");
        };
        WriteValidatedSaveFile(root_/"population-state-v1.json",checkpoint.json,validate,files_);
        return {true,{}};
    }catch(const std::exception& e){return {false,e.what()};}
}
}
