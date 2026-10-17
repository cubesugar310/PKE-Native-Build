#include "pke/ApplicationFlow.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace pke {
ApplicationFlow::ApplicationFlow(AssetService& assets,NativeSaveSummary player)
    :assets_(assets),player_(std::move(player)),catalog_(assets.Catalog()){
    if(!SkinAvailable(player_.selectedSkin)){
        player_.selectedSkin.clear();for(const auto& c:catalog_->characters)if(SkinAvailable(c.id)){player_.selectedSkin=c.sourceSkinId.empty()?c.id:c.sourceSkinId;break;}
    }
}
void ApplicationFlow::Filter(){
    visible_=assets_.FilterSongs(search_,side_,stars_);
    if(!selected_||std::find(visible_.begin(),visible_.end(),*selected_)==visible_.end())
        selected_=visible_.empty()?std::nullopt:std::optional<std::size_t>{visible_.front()};
}
void ApplicationFlow::OpenPractice(){
    if(screen_==AppScreen::Home){search_.clear();stars_=0;}
    screen_=AppScreen::Practice;Filter();
}
void ApplicationFlow::OpenHalloween(){if(screen_==AppScreen::Home)screen_=AppScreen::Halloween;}
void ApplicationFlow::Back(){
    if(screen_==AppScreen::Gameplay||screen_==AppScreen::Result){screen_=AppScreen::Practice;Filter();}
    else screen_=AppScreen::Home;
}
void ApplicationFlow::SetSearch(std::string value){search_=std::move(value);Filter();}
void ApplicationFlow::SetStars(int stars){stars_=stars>=1&&stars<=7?stars:0;Filter();}
void ApplicationFlow::SetSide(Side side){side_=side;Filter();}
void ApplicationFlow::SelectSong(std::size_t index){if(std::find(visible_.begin(),visible_.end(),index)!=visible_.end())selected_=index;}
void ApplicationFlow::MoveSelection(int direction){
    if(visible_.empty())return;
    const auto found=selected_?std::find(visible_.begin(),visible_.end(),*selected_):visible_.begin();
    const auto position=found==visible_.end()?0:std::distance(visible_.begin(),found);
    const auto next=std::clamp<std::ptrdiff_t>(position+direction,0,static_cast<std::ptrdiff_t>(visible_.size())-1);
    selected_=visible_.at(static_cast<std::size_t>(next));
}
bool ApplicationFlow::BeginGameplay(){
    if((screen_!=AppScreen::Practice&&screen_!=AppScreen::Result)||!selected_||*selected_>=catalog_->songs.size())return false;
    screen_=AppScreen::Gameplay;return true;
}
void ApplicationFlow::FinishGameplay(const PlayStats& stats){
    if(screen_!=AppScreen::Gameplay||!selected_)return;
    result_={catalog_->songs.at(*selected_).displayName,stats,side_};screen_=AppScreen::Result;
}
void ApplicationFlow::SetPlayerName(std::string value){
    const auto begin=value.find_first_not_of(" \t\r\n");if(begin==std::string::npos)return;
    value=value.substr(begin,value.find_last_not_of(" \t\r\n")-begin+1);
    std::size_t end=0;int units=0;
    while(end<value.size()&&units<24){const auto c=static_cast<unsigned char>(value[end]);
        const std::size_t width=c<0x80?1:c<0xe0?2:c<0xf0?3:4;const int count=width==4?2:1;
        if(units+count>24||end+width>value.size())break;end+=width;units+=count;}
    player_.playerName=value.substr(0,end);
}
void ApplicationFlow::SetSkin(std::string id){
    if(SkinAvailable(id))player_.selectedSkin=std::move(id);
}
void ApplicationFlow::RefreshCatalog(){catalog_=assets_.Catalog();Filter();}
HalloweenWindow ApplicationFlow::Halloween(std::int64_t now) const{
    using namespace std::chrono;
    const auto instant=sys_time<milliseconds>{milliseconds{now}};
    const int year=int(year_month_day{floor<days>(instant+hours{9})}.year());
    const auto start=sys_days{std::chrono::year{year}/October/14}+hours{15};
    const auto end=sys_days{std::chrono::year{year}/October/31}+hours{15};
    bool active=instant>=start&&instant<end;
    if(developerEnabled_&&player_.halloweenDev.eventOverride!="auto")active=player_.halloweenDev.eventOverride=="force-on";
    return {active,year,active?std::max<std::int64_t>(0,duration_cast<milliseconds>(end-instant).count()):0};
}
bool ApplicationFlow::EnableDeveloper(std::string_view code,std::string_view pin){
    if(code=="cubesugar"&&pin=="A38UVGD6")developerEnabled_=true;return developerEnabled_;
}
std::vector<HalloweenShopEntry> ApplicationFlow::HalloweenShop() const{
    std::vector<HalloweenShopEntry> result;std::vector<std::string> seen;
    for(std::size_t i=0;i<4;++i){const auto& slot=player_.halloweenDev.skinSlots[i];
        if(slot.skinId.empty()||std::find(seen.begin(),seen.end(),slot.skinId)!=seen.end())continue;
        seen.push_back(slot.skinId);
        const auto* found=FindCharacterAsset(*catalog_,slot.skinId);
        if(found)result.push_back({i,slot.skinId,std::clamp(slot.price,0,999999),found});
    }return result;
}
bool ApplicationFlow::SkinAvailable(std::string_view id) const{
    const auto* character=FindCharacterAsset(*catalog_,id);if(!character)return false;
    const auto entries=HalloweenShop();return std::none_of(entries.begin(),entries.end(),[&](const auto& e){return e.character==character&&std::find(player_.purchasedSkinIds.begin(),player_.purchasedSkinIds.end(),e.skinId)==player_.purchasedSkinIds.end();});
}
std::string ApplicationFlow::PurchaseHalloweenTitle(std::int64_t now,const PersistPlayer& persist){
    if(!Halloween(now).active)return "inactive";
    if(std::find(player_.unlockedTitleIds.begin(),player_.unlockedTitleIds.end(),"trick-or-beat")!=player_.unlockedTitleIds.end())return "owned";
    if(player_.halloweenCandy<5000)return "insufficient";
    auto next=player_;next.halloweenCandy-=5000;next.unlockedTitleIds.push_back("trick-or-beat");
    auto& purchases=next.halloweenHistory[std::to_string(Halloween(now).year)].purchases;
    if(std::find(purchases.begin(),purchases.end(),"trick-or-beat")==purchases.end())purchases.push_back("trick-or-beat");
    if(!persist(next))return "error";player_=std::move(next);return "purchased";
}
std::string ApplicationFlow::PurchaseHalloweenSkin(std::size_t slot,std::int64_t now,const PersistPlayer& persist){
    if(!Halloween(now).active)return "inactive";
    const auto entries=HalloweenShop();const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.slotIndex==slot;});
    if(found==entries.end())return "unavailable";
    if(std::find(player_.purchasedSkinIds.begin(),player_.purchasedSkinIds.end(),found->skinId)!=player_.purchasedSkinIds.end())return "owned";
    if(player_.halloweenCandy<static_cast<std::uint64_t>(found->price))return "insufficient";
    auto next=player_;next.halloweenCandy-=found->price;next.purchasedSkinIds.push_back(found->skinId);
    auto& purchases=next.halloweenHistory[std::to_string(Halloween(now).year)].purchases;
    if(std::find(purchases.begin(),purchases.end(),found->skinId)==purchases.end())purchases.push_back(found->skinId);
    if(!persist(next))return "error";player_=std::move(next);return "purchased";
}
bool ApplicationFlow::SavePreferences(double volume,std::string scroll,std::string judgement,const PersistPlayer& persist){
    if(!std::isfinite(volume)||volume<0||volume>1||(scroll!="up"&&scroll!="down")||(judgement!="center"&&judgement!="corner"))return false;
    auto next=player_;next.masterVolume=volume;next.scrollDirection=std::move(scroll);next.judgementPosition=std::move(judgement);
    if(!persist(next))return false;player_=std::move(next);return true;
}
bool ApplicationFlow::SaveEventOverride(std::string value,const PersistPlayer& persist){
    if(!developerEnabled_||(value!="auto"&&value!="force-on"&&value!="force-off"))return false;
    auto next=player_;next.halloweenDev.eventOverride=std::move(value);
    if(!persist(next))return false;player_=std::move(next);return true;
}
}
