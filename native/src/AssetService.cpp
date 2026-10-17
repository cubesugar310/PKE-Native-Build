#include "pke/AssetService.hpp"
#include "pke/CharacterLoader.hpp"
#include "pke/ChartParser.hpp"
#include "pke/QuickJsRuntime.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#ifdef PKE_NATIVE_USE_NLOHMANN
#include <nlohmann/json.hpp>
#include <tinyxml2.h>
#else
#include <boost/json.hpp>
#include <boost/property_tree/xml_parser.hpp>
#endif

namespace pke {
namespace {
#ifdef PKE_NATIVE_USE_NLOHMANN
using Json=nlohmann::json;
Json Parse(const std::string& s){return Json::parse(s);}
const Json& At(const Json& j,const char* key){return j.at(key);}
std::string Text(const Json& j){return j.get<std::string>();}
double Number(const Json& j){return j.get<double>();}
std::string Dump(const Json& j){return j.dump();}
const Json& Array(const Json& j){if(!j.is_array())throw std::runtime_error("Invalid catalog array");return j;}
#else
using Json=boost::json::value;
Json Parse(const std::string& s){return boost::json::parse(s);}
const Json& At(const Json& j,const char* key){return j.as_object().at(key);}
std::string Text(const Json& j){return std::string(j.as_string());}
double Number(const Json& j){if(j.is_int64())return static_cast<double>(j.as_int64());if(j.is_uint64())return static_cast<double>(j.as_uint64());return j.as_double();}
std::string Dump(const Json& j){return boost::json::serialize(j);}
const boost::json::array& Array(const Json& j){return j.as_array();}
#endif
int Integer(const Json& j){const auto n=Number(j);if(!std::isfinite(n)||std::floor(n)!=n||n<-1||n>10000000)throw std::runtime_error("Invalid catalog integer");return static_cast<int>(n);}
std::string Utf8(const std::filesystem::path& p){const auto text=p.generic_u8string();return {reinterpret_cast<const char*>(text.data()),text.size()};}
std::string Lower(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c+=('a'-'A');return value;}
std::string Quote(std::string_view s){
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=c;}
        else if(c<0x20){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=c;}
    return out+'"';
}
std::string Read(const std::filesystem::path& p){
    std::ifstream in(p,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot open asset");
    const auto size=in.tellg();if(size<0||size>64*1024*1024)throw std::runtime_error("Asset exceeds 64 MiB limit");
    std::string text(static_cast<std::size_t>(size),'\0');in.seekg(0);
    if(!in.read(text.data(),size))throw std::runtime_error("Cannot read complete asset");return text;
}
std::string Require(QuickJsResult result){if(!result.ok)throw std::runtime_error(result.error);return std::move(result.json);}
std::vector<std::filesystem::path> Files(const std::filesystem::path& root,std::vector<AssetIssue>& issues){
    std::vector<std::filesystem::path> result;std::error_code error;
    std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,error),end;
    if(error){issues.push_back({root,error.message()});return result;}
    while(it!=end){
        const auto entry=*it;std::error_code typeError;
        if(entry.is_regular_file(typeError))result.push_back(entry.path());
        if(typeError)issues.push_back({entry.path(),typeError.message()});
        it.increment(error);if(error){issues.push_back({root,error.message()});error.clear();}
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return Utf8(a)<Utf8(b);});return result;
}
std::string FileMetadata(const std::vector<std::filesystem::path>& files,const std::filesystem::path& root,std::size_t offset=0){
    std::string result="[";for(std::size_t i=0;i<files.size();++i){if(i)result+=',';
        result+="{\"key\":"+std::to_string(offset+i)+",\"name\":"+Quote(Utf8(files[i].filename()))+
            ",\"pkeRelativePath\":"+Quote(Utf8(files[i].lexically_relative(root)))+'}';}
    return result+']';
}
std::size_t Index(const Json& j,std::size_t size){const auto n=Integer(j);if(n<0||static_cast<std::size_t>(n)>=size)throw std::runtime_error("Invalid catalog file index");return static_cast<std::size_t>(n);}
std::string Difficulty(const std::filesystem::path& chart,std::string_view folder){
    auto stem=Utf8(chart.stem());const auto prefix=Lower(std::string(folder))+"-";
    if(Lower(stem).starts_with(prefix))stem.erase(0,prefix.size());return stem;
}
std::string AtlasImagePath(const std::filesystem::path& path){
    const auto text=Read(path);
#ifdef PKE_NATIVE_USE_NLOHMANN
    tinyxml2::XMLDocument document;
    if(document.Parse(text.data(),text.size())!=tinyxml2::XML_SUCCESS)throw std::runtime_error("Invalid character XML");
    const auto* root=document.FirstChildElement("TextureAtlas");if(!root)throw std::runtime_error("TextureAtlas missing");
    const auto* image=root->Attribute("imagePath");return image?image:"";
#else
    boost::property_tree::ptree document;std::istringstream input(text);
    boost::property_tree::read_xml(input,document);
    return document.get_child("TextureAtlas").get<std::string>("<xmlattr>.imagePath","");
#endif
}
}

struct AssetService::Impl {
    std::filesystem::path songsRoot,charactersRoot;
    QuickJsRuntime vm;
    std::shared_ptr<const AssetCatalog> catalog=std::make_shared<AssetCatalog>();
    Impl(const Config& config,const std::filesystem::path& exe,const std::filesystem::path& compat,const std::string& locale){
        songsRoot=(config.songsRoot.is_absolute()?config.songsRoot:exe/config.songsRoot).lexically_normal();
        charactersRoot=(config.charactersRoot.is_absolute()?config.charactersRoot:exe/config.charactersRoot).lexically_normal();
        Require(vm.InstallLocaleCompare(locale));
        for(const auto* f:{"chart-utils.js","song-difficulty.js","catalog-imports.js","catalog.js"})Require(vm.Evaluate(Read(compat/f),f));
    }
    Json Call(const char* name,const std::string& input){return Parse(Require(vm.CallJson(name,input)));}
};
AssetService::AssetService(const Config& config,std::filesystem::path exe,std::filesystem::path compat,std::string locale)
    :impl_(std::make_unique<Impl>(config,exe,compat,locale)){}
AssetService::~AssetService()=default;
std::shared_ptr<const AssetCatalog> AssetService::Catalog() const noexcept{return impl_->catalog;}
std::shared_ptr<const AssetCatalog> AssetService::Refresh(){
    auto next=std::make_shared<AssetCatalog>();next->generation=impl_->catalog->generation+1;
    const auto characterFiles=Files(impl_->charactersRoot,next->issues);
    const auto characterMetadata=FileMetadata(characterFiles,impl_->charactersRoot.parent_path());
    impl_->Call("pkeCatalogSetCharacterFiles","{\"files\":"+characterMetadata+'}');
    std::map<std::string,std::size_t> selectedCharacterFiles;
    for(std::size_t fileIndex=0;fileIndex<characterFiles.size();++fileIndex){
        const auto& file=characterFiles[fileIndex];
        if(Lower(Utf8(file.extension()))!=".json")continue;
        try{
            const auto plan=impl_->Call("pkeCatalogCharacterPlan","{\"text\":"+Quote(Read(file))+
                ",\"key\":"+std::to_string(fileIndex)+'}');
            if(plan.is_null())continue;
            const auto id=Text(At(plan,"id"));
            CharacterAssetPaths paths;paths.json=file;paths.xml=characterFiles.at(Index(At(plan,"xml"),characterFiles.size()));
            const auto image=impl_->Call("pkeCatalogCharacterImage","{\"directory\":"+Dump(At(plan,"directory"))+
                ",\"imageBase\":"+Dump(At(plan,"imageBase"))+",\"xmlImagePath\":"+Quote(AtlasImagePath(paths.xml))+'}');
            if(Integer(image)<0)throw std::runtime_error("Character image is missing");
            paths.image=characterFiles.at(Index(image,characterFiles.size()));
            auto definition=std::make_shared<CharacterDefinition>(LoadCharacter(paths));
            if(definition->atlas.empty()||definition->imagePath.empty()||!std::filesystem::is_regular_file(definition->imagePath))
                throw std::runtime_error("Character atlas or image is missing");
            const auto existing=std::find_if(next->characters.begin(),next->characters.end(),[&](const auto& c){return c.id==id;});
            if(existing!=next->characters.end()){
                const auto prefer=impl_->Call("pkeCatalogPreferCharacter","{\"candidate\":"+std::to_string(fileIndex)+
                    ",\"current\":"+std::to_string(selectedCharacterFiles.at(id))+'}');
                if(Dump(prefer)!="true")continue;
                *existing={id,Text(At(plan,"displayName")),file,std::move(definition)};
            }else next->characters.push_back({id,Text(At(plan,"displayName")),file,std::move(definition)});
            selectedCharacterFiles[id]=fileIndex;
        }catch(const std::exception& e){next->issues.push_back({file,e.what()});}
    }
    const auto songFiles=Files(impl_->songsRoot,next->issues);
    std::map<std::filesystem::path,std::vector<std::filesystem::path>> grouped;
    for(const auto& file:songFiles){const auto relative=file.lexically_relative(impl_->songsRoot);
        if(relative.begin()==relative.end())continue;auto part=relative.begin();const auto first=*part;
        if(++part!=relative.end())grouped[first].push_back(file);}
    for(const auto& [folder,files]:grouped){
        const auto folderName=Utf8(folder);const auto metadata=FileMetadata(files,impl_->songsRoot/folder);
        try{
            const auto plan=impl_->Call("pkeCatalogFilePlan","{\"folder\":"+Quote(folderName)+",\"files\":"+metadata+'}');
            if(Integer(At(plan,"inst"))<0)continue;
            SongAsset song;song.folderName=folderName;song.folder=impl_->songsRoot/folder;
            song.instPath=files.at(Index(At(plan,"inst"),files.size()));
            if(Integer(At(plan,"voices"))>=0)song.voicesPath=files.at(Index(At(plan,"voices"),files.size()));
            for(const auto& candidate:Array(At(plan,"charts"))){
                const auto path=files.at(Index(candidate,files.size()));
                try{
                    const auto value=impl_->Call("pkeCatalogChart","{\"text\":"+Quote(Read(path))+",\"fallback\":"+Quote(folderName)+'}');
                    if(value.is_null())throw std::runtime_error("Playable notes not found");
                    const auto& light=At(value,"beatBattleLightweightChart");const auto& rating=At(value,"beatBattleDifficulty");
                    SongChartAsset chart;chart.path=path;chart.difficulty=Difficulty(path,folderName);
                    chart.playerStars=Integer(At(At(rating,"player"),"stars"));chart.opponentStars=Integer(At(At(rating,"opponent"),"stars"));
                    chart.playerNoteCount=Integer(At(light,"playerNoteCount"));chart.opponentNoteCount=Integer(At(light,"opponentNoteCount"));
                    chart.durationMs=Number(At(light,"durationMs"));chart.baseBpm=Number(At(light,"baseBpm"));
                    if(!std::isfinite(chart.durationMs)||!std::isfinite(chart.baseBpm))throw std::runtime_error("Nonfinite chart clock");
                    if(!At(rating,"scrollSpeed").is_null())chart.scrollSpeed=Number(At(rating,"scrollSpeed"));
                    chart.lightweightJson=Dump(light);chart.difficultyJson=Dump(rating);
                    // The reference-normalized authored heads are shared by
                    // discovery and gameplay. Retain parser tempo/character
                    // metadata, replacing its prototype head interpretation.
                    try{chart.chart=ParseChart(path);}catch(const std::exception& e){chart.chart.warnings.push_back(e.what());}
                    chart.chart.notes.clear();
                    const auto& times=Array(At(light,"times"));const auto& sides=Array(At(light,"sides"));
                    const auto& lanes=Array(At(light,"lanes"));const auto& lengths=Array(At(light,"lengths"));
                    for(std::size_t i=0;i<times.size();++i){
                        const auto time=Number(times.at(i)),length=Number(lengths.at(i));
                        if(!std::isfinite(time)||!std::isfinite(length))throw std::runtime_error("Nonfinite authored note");
                        chart.chart.notes.push_back({time,Integer(lanes.at(i)),length,Integer(sides.at(i))?Side::Opponent:Side::Player});
                    }
                    if(chart.chart.bpmTimeline.empty()){chart.chart.baseBpm=chart.baseBpm;chart.chart.bpmTimeline.push_back({0,chart.baseBpm});}
                    chart.chart.scrollSpeed=chart.scrollSpeed.value_or(1.8);
                    if(song.charts.empty()){song.displayName=Text(At(value,"displayName"));song.opponentCharacterId=Text(At(value,"opponentCharacterId"));}
                    song.charts.push_back(std::move(chart));
                }catch(const std::exception& e){next->issues.push_back({path,e.what()});}
            }
            if(song.charts.empty())continue;
            const auto icon=impl_->Call("pkeCatalogIcon","{\"opponent\":"+Quote(song.opponentCharacterId)+",\"files\":"+metadata+'}');
            if(Integer(icon)>=0){const auto index=Index(icon,files.size()+characterFiles.size());
                song.iconPath=index<files.size()?files[index]:characterFiles[index-files.size()];}
            next->songs.push_back(std::move(song));
        }catch(const std::exception& e){next->issues.push_back({impl_->songsRoot/folder,e.what()});}
    }
    std::string orderInput="{\"songs\":[";
    for(std::size_t i=0;i<next->songs.size();++i){if(i)orderInput+=',';orderInput+="{\"key\":"+std::to_string(i)+",\"displayName\":"+Quote(next->songs[i].displayName)+'}';}
    const auto order=impl_->Call("pkeCatalogSort",orderInput+"]}");std::vector<SongAsset> sorted;sorted.reserve(next->songs.size());
    for(const auto& index:Array(order))sorted.push_back(std::move(next->songs.at(Index(index,next->songs.size()))));next->songs=std::move(sorted);
    std::string indexInput="{\"songs\":[";
    for(std::size_t i=0;i<next->songs.size();++i){const auto& song=next->songs[i];if(i)indexInput+=',';
        indexInput+="{\"displayName\":"+Quote(song.displayName)+",\"folderName\":"+Quote(song.folderName)+
            ",\"chartFile\":{\"name\":"+Quote(Utf8(song.charts.front().path.filename()))+"},\"beatBattleDifficulty\":"+song.charts.front().difficultyJson+'}';}
    impl_->Call("pkeCatalogSetSearch",indexInput+"]}");
    impl_->catalog=next;Require(impl_->vm.CollectGarbage());return next;
}
std::vector<std::size_t> AssetService::FilterSongs(std::string_view query,Side side,int stars){
    const auto result=impl_->Call("pkeCatalogSearch","{\"query\":"+Quote(query)+",\"side\":"+
        Quote(side==Side::Opponent?"opponent":"player")+",\"stars\":"+std::to_string(stars)+'}');
    std::vector<std::size_t> indices;for(const auto& index:Array(result))indices.push_back(Index(index,impl_->catalog->songs.size()));return indices;
}
std::string AssetService::PopulationCatalogJson() const{
    std::string result="{\"songs\":[";
    for(std::size_t i=0;i<impl_->catalog->songs.size();++i){const auto& s=impl_->catalog->songs[i];if(i)result+=',';
        result+="{\"folderName\":"+Quote(s.folderName)+",\"displayName\":"+Quote(s.displayName)+
            ",\"opponentCharacterId\":"+Quote(s.opponentCharacterId)+",\"chartFile\":{\"name\":"+Quote(Utf8(s.charts.front().path.filename()))+
            "},\"instFile\":{\"name\":"+Quote(Utf8(s.instPath.filename()))+"},\"beatBattleLightweightChart\":"+s.charts.front().lightweightJson+
            ",\"beatBattleDifficulty\":"+s.charts.front().difficultyJson+'}';}
    result+="],\"skins\":[";
    for(std::size_t i=0;i<impl_->catalog->characters.size();++i){if(i)result+=',';result+=Quote(impl_->catalog->characters[i].id);}
    return result+"]}";
}
}
