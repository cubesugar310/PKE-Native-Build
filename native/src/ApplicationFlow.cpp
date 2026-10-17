#include "pke/ApplicationFlow.hpp"
#include <algorithm>

namespace pke {
ApplicationFlow::ApplicationFlow(AssetService& assets,NativeSaveSummary player)
    :assets_(assets),player_(std::move(player)),catalog_(assets.Catalog()){}
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
    if(std::any_of(catalog_->characters.begin(),catalog_->characters.end(),[&](const auto& item){return item.id==id;}))player_.selectedSkin=std::move(id);
}
void ApplicationFlow::RefreshCatalog(){catalog_=assets_.Catalog();Filter();}
}
