#include "pke/ApplicationView.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace pke {
namespace {
constexpr UiColor white{255,255,255,255},gold{255,226,135,255},lavender{240,213,255,255};
constexpr SDL_Rect back{1054,32,194,44},side{168,112,146,40},search{430,112,360,40},stars{918,112,146,40};
constexpr SDL_Rect nameBox{970,432,198,34},saveName{1174,432,66,34};
constexpr int rowY=222,rowHeight=78,rowCount=5;
constexpr std::array<const char*,12> homeLabels{"オンラインバトル開始!","プラクティスモード","別マッチを観戦","Change Songs Folder","Change Characters Folder","ランキング","戦績","FRIENDS","PRIVATE MATCH","BOSS BATTLE","設定","リプレイ"};
std::array<SDL_Rect,12> HomeRects(){
    constexpr double weights[]{1.15,1,1,.82,.82,.76,.7,.68,.7};
    std::array<SDL_Rect,12> result{};double total=0;for(const auto weight:weights)total+=weight;
    for(int row=0;row<2;++row){double x=18;for(int column=0;column<9;++column){const int index=row*9+column;if(index>=12)break;
        const double width=(1244-80)*weights[column]/total;result[index]={int(std::lround(x)),550+row*76,int(std::lround(width)),66};x+=width+10;}}
    return result;
}
std::string Number(int value){return std::to_string(value);}
std::string Percentage(double value){char text[32];std::snprintf(text,sizeof(text),"%.2f%%",value);return text;}
}
void ApplicationView::Button(std::string_view id,std::string_view label,SDL_Rect rect,UiColor top,UiColor bottom,int size){
    ui_.Card(id,rect,top,bottom,14,3);rect.x+=8;rect.w-=16;
    const auto ink=(int(top.r)+top.g+top.b)>410?UiColor{24,17,30,255}:white;
    ui_.Text(std::string(id)+"-text",label,rect,size,ink,true);
}
void ApplicationView::Prepare(const ApplicationFlow& flow){
    const auto catalog=flow.Catalog();std::string id=flow.Player().selectedSkin;const auto colon=id.find_last_of(':');if(colon!=std::string::npos)id.erase(0,colon+1);
    const CharacterAsset* chosen=nullptr;
    for(const auto& c:catalog->characters)if(c.id==id){chosen=&c;break;}
    if(!chosen&&!catalog->characters.empty())chosen=&catalog->characters.front();
    if(chosen&&(chosen->id!=previewId_||chosen->definition.get()!=preview_)){
        previewId_=chosen->id;preview_=chosen->definition.get();animator_.SetCharacter(preview_);ui_.PreloadImage("home-skin",preview_->imagePath);
    }else if(!chosen){preview_=nullptr;previewId_.clear();}
    for(int row=0;row<rowCount;++row){const auto position=firstRow_+row;if(position<0||position>=static_cast<int>(flow.VisibleSongs().size()))continue;
        const auto& song=catalog->songs[flow.VisibleSongs()[position]];
        if(!song.iconPath.empty())ui_.PreloadImage("practice-icon-"+Number(row),song.iconPath);
    }
}
void ApplicationView::Scroll(const ApplicationFlow& flow,int rows){firstRow_=std::clamp(firstRow_+rows,0,std::max(0,int(flow.VisibleSongs().size())-rowCount));Prepare(flow);}
void ApplicationView::EnsureSelected(const ApplicationFlow& flow){
    const auto selected=flow.SelectedSong();if(!selected){firstRow_=0;return;}
    const auto found=std::find(flow.VisibleSongs().begin(),flow.VisibleSongs().end(),*selected);
    const int position=int(std::distance(flow.VisibleSongs().begin(),found));
    if(position<firstRow_)firstRow_=position;else if(position>=firstRow_+rowCount)firstRow_=position-rowCount+1;
    firstRow_=std::clamp(firstRow_,0,std::max(0,int(flow.VisibleSongs().size())-rowCount));Prepare(flow);
}
void ApplicationView::Home(const ApplicationFlow& flow,const PopulationSummary* population,const PopulationStatus* state,double time){
    ui_.Begin({92,0,8,255});ui_.Panel("home-background",{0,0,1280,720},{92,0,8,255},{103,8,15,255});
    ui_.Card("logo",{18,18,280,78},{255,218,65,255},{255,159,31,255},16,3);
    ui_.Text("logo-name","PKE Beat Battle",{36,31,244,34},22,{24,24,24,255});
    ui_.Text("logo-sub","ONLINE TITLE MATCH",{36,67,244,20},13,{24,24,24,255});
    ui_.Card("network",{840,18,422,92},{115,70,255,255},{78,35,201,255},14,3);
    ui_.Text("network-label","NETWORK STATUS",{858,29,386,20},12,white);
    std::string count=population?"REGISTERED "+Number(population->registered)+" / ONLINE "+Number(population->online):"REGISTERED ---- / ONLINE ----";
    ui_.Text("network-count",count,{858,50,386,25},18,white);
    ui_.Text("network-live",population?"LIVE "+Number(population->live)+" / "+Number(population->liveCap)+"  WAIT "+Number(population->waiting):"CHECKING...",{858,79,386,20},13,white);
    ui_.Card("hero",{160,140,620,356},{109,40,251,250},{92,28,223,250},22,3);
    ui_.Text("ready",flow.Catalog()->songs.empty()?"NOT READY":"READY!",{320,112,300,36},24,gold,true);
    if(preview_){animator_.Update(time);ui_.Character("home-skin",*preview_,animator_.CurrentFrame(),{190,157,560,220});}
    else ui_.Text("preview-fallback","BF",{190,157,560,220},50,white,true);
    ui_.Text("match-detail","ランダムマッチングでバトル開始",{184,379,572,24},15,white,true);
    ui_.Text("asset-count",Number(int(flow.Catalog()->songs.size()))+" SONGS READY    "+Number(int(flow.Catalog()->characters.size()))+" CHARACTERS READY",{184,410,572,22},12,white,true);
    ui_.Text("range",flow.Player().progress.onlineSongRangeTier=="all"?"全曲（★1〜★7）":flow.Player().progress.onlineSongRangeTier=="standard"?"標準（★1〜★5）":"初心者保護（★1〜★3）",{184,444,572,26},16,white,true);
    ui_.Card("player-card",{944,132,318,114},{19,12,28,235},{19,12,28,235},16,3);
    ui_.Text("season","ONLINE BATTLE • SEASON 2",{962,144,282,23},15,gold);
    ui_.Text("player-rp","PLAYER RP "+Number(flow.Player().progress.rp),{962,177,282,25},20,white);
    ui_.Text("player-record","W "+Number(flow.Player().progress.wins)+" / L "+Number(flow.Player().progress.losses)+" / D "+Number(flow.Player().progress.draws),{962,211,282,22},14,white);
    ui_.Card("setup",{944,258,318,77},{19,12,28,235},{19,12,28,235},16,3);
    ui_.Text("setup-label","SETUP",{962,270,282,20},15,gold);
    ui_.Text("setup-count","Songs: "+Number(int(flow.Catalog()->songs.size()))+"  Characters: "+Number(int(flow.Catalog()->characters.size())),{962,298,282,22},13,white);
    ui_.Card("skin",{944,347,318,151},{19,12,28,235},{19,12,28,235},16,3);
    ui_.Text("skin-label","MY SKIN",{962,359,282,20},15,gold);
    Button("previous","◀ PREV",{960,386,82,32},{41,32,54,255},{41,32,54,255},11);
    Button("next","NEXT ▶",{1162,386,82,32},{41,32,54,255},{41,32,54,255},11);
    ui_.Text("skin-name",previewId_.empty()?"未選択":previewId_,{1048,387,108,30},13,white,true);
    ui_.Panel("name-box",nameBox,{255,255,255,240},{255,255,255,240},8);
    ui_.Text("name",nameFocus?nameEdit+composition:flow.Player().playerName,{978,437,182,25},16,{23,17,30,255});
    Button("save-name","保存",saveName,{255,231,131,255},{233,182,47,255},14);
    ui_.Text("name-help","Enter または 保存 で変更 • 最大24文字",{962,474,282,17},10,{184,170,200,255});
    const auto rects=HomeRects();
    for(std::size_t i=0;i<rects.size();++i){UiColor top{227,160,255,255},bottom{189,89,234,255};
        if(i==0){top={255,179,39,255};bottom={255,123,0,255};}
        else if(i==2){top={45,214,179,255};bottom={18,152,120,255};}
        else if(i>2){top={235,228,247,255};bottom={190,175,210,255};}
        Button("home-button-"+Number(int(i)),homeLabels[i],rects[i],top,bottom,14);
    }
    if(state&&state->phase==PopulationPhase::Failed)ui_.Text("population-error",state->error,{18,695,1244,22},12,gold);
    else if(!status.empty())ui_.Text("status",status,{18,695,1244,22},12,gold);
}
void ApplicationView::Practice(const ApplicationFlow& flow){
    ui_.Begin({21,17,34,255});ui_.Panel("practice-background",{0,0,1280,720},{81,43,114,255},{21,17,34,255});
    ui_.Text("freeplay","FREEPLAY",{32,26,600,24},12,{234,186,255,255});
    ui_.Text("practice-heading","プラクティスモード",{32,51,720,44},32,white);
    Button("back","タイトルへ / Esc",back,lavender,lavender,16);
    ui_.Text("side-label","プレイする側",{32,120,132,27},16,lavender);
    Button("side",flow.PracticeSide()==Side::Player?"BF側":"DAD側",side,{41,32,54,255},{41,32,54,255},16);
    ui_.Text("search-label","曲検索",{344,120,82,27},16,lavender);
    ui_.Card("search",search,{33,26,45,255},{33,26,45,255},9,2);
    ui_.Text("search-text",flow.Search().empty()&&!searchFocus?"曲名を検索":flow.Search()+composition,{442,121,336,27},14,flow.Search().empty()&&!searchFocus?UiColor{169,147,188,255}:white);
    ui_.Text("star-label","★絞り込み",{812,120,102,27},16,lavender);
    Button("stars",flow.Stars()?"★"+Number(flow.Stars()):"すべて",stars,{41,32,54,255},{41,32,54,255},16);
    ui_.Text("practice-help","曲とプレイする側を選んでひとりで練習。反対側のCPUは0msで自動演奏・得点なし。RP・平均精度・戦績には影響しません。",{32,169,1216,24},13,{212,196,230,255});
    const auto visible=flow.VisibleSongs().size(),total=flow.Catalog()->songs.size();
    ui_.Text("practice-status",status.empty()?Number(int(visible))+" / "+Number(int(total))+"曲 • クリックまたはEnterで練習開始":status,{32,195,1216,24},13,gold);
    for(int row=0;row<rowCount;++row){const auto position=firstRow_+row;if(position>=int(visible))break;
        const auto index=flow.VisibleSongs()[position];const auto& song=flow.Catalog()->songs[index];
        const bool selected=flow.SelectedSong()==std::optional<std::size_t>{index};
        const SDL_Rect rect{36,rowY+row*rowHeight,1200,68};const auto color=selected?UiColor{82,49,100,255}:UiColor{41,32,54,255};
        ui_.Card("song-"+Number(row),rect,color,color,14,selected?4:2);
        ui_.Text("song-name-"+Number(row),song.displayName,{56,rect.y+9,1096,29},25,white);
        const auto& chart=song.charts.front();
        ui_.Text("song-stars-"+Number(row),"BF ★"+Number(chart.playerStars)+" / DAD ★"+Number(chart.opponentStars)+" • Notes "+Number(chart.playerNoteCount)+" / "+Number(chart.opponentNoteCount),{56,rect.y+43,1096,21},12,lavender);
        ui_.Image("practice-icon-"+Number(row),{1170,rect.y+10,48,48},true);
    }
    if(!visible)ui_.Text("no-songs",total?"該当する曲がありません":"曲がありません。タイトルのChange Songs Folderで曲フォルダを読み込んでください。",{32,290,1216,90},20,lavender,true);
    ui_.Text("practice-footer","↑ ↓ で曲選択 / Enterでプレイ / プレイ中のEscで曲一覧に戻る",{32,674,1216,26},13,{212,196,230,255});
    if(sideMenu)for(int i=0;i<2;++i)Button("side-choice-"+Number(i),i?"DAD側":"BF側",{side.x,side.y+44+i*40,side.w,40},{41,32,54,255},{41,32,54,255},16);
    if(starMenu)for(int i=0;i<8;++i)Button("star-choice-"+Number(i),i?"★"+Number(i):"すべて",{stars.x,stars.y+44+i*40,stars.w,40},{41,32,54,255},{41,32,54,255},16);
}
void ApplicationView::Result(const ApplicationFlow& flow){
    ui_.Begin({15,12,21,255});ui_.Card("result",{360,164,560,392},{57,38,72,255},{18,14,25,255},24,4);
    ui_.Text("result-eyebrow","PRACTICE RESULT",{388,190,504,23},12,lavender,true);
    ui_.Text("result-heading","PRACTICE RESULT",{388,218,504,60},40,{234,186,255,255},true);
    ui_.Text("result-song",flow.Result().songName,{388,283,504,34},20,white,true);
    const auto& s=flow.Result().stats;const std::array<std::string,4> values{Number(s.sick),Number(s.good),Number(s.bad),Percentage(s.accuracyPercent)};
    const std::array<const char*,4> labels{"SICK","GOOD","BAD","ACCURACY"};
    for(int i=0;i<4;++i){SDL_Rect rect{388+(i%2)*257,327+(i/2)*71,247,61};ui_.Panel("result-stat-"+Number(i),rect,{255,255,255,15},{255,255,255,15},12);
        ui_.Text("result-label-"+Number(i),labels[i],{rect.x,rect.y+6,rect.w,16},11,{184,166,200,255},true);
        ui_.Text("result-value-"+Number(i),values[i],{rect.x,rect.y+24,rect.w,32},24,white,true);}
    Button("replay-save",replaySaved?"保存済み":"リプレイを保存",{388,485,247,43},{82,49,100,255},{82,49,100,255},16);
    Button("result-back","曲選択に戻る",{645,485,247,43},lavender,lavender,16);
    if(!status.empty())ui_.Text("result-status",status,{360,570,560,60},14,gold,true);
}
void ApplicationView::Draw(const ApplicationFlow& flow,const PopulationSummary* population,const PopulationStatus* state,double animationMs){
    if(flow.Screen()==AppScreen::Home)Home(flow,population,state,animationMs);
    else if(flow.Screen()==AppScreen::Practice)Practice(flow);
    else if(flow.Screen()==AppScreen::Result)Result(flow);
    else {ui_.Begin({21,17,34,255});Button("back","タイトルへ / Esc",back,lavender,lavender,16);ui_.Text("event","HALLOWEEN",{80,64,800,60},40,gold);}
    if(!persistenceError.empty()){
        ui_.Panel("persistence-error-background",{14,691,1252,29},{21,17,34,255},{21,17,34,255});
        ui_.Text("persistence-error",persistenceError,{18,695,1244,22},12,gold);
    }
    ui_.End();
}
void ApplicationView::GameplayOverlay(){ui_.BeginOverlay();Button("game-back","曲選択に戻る / Esc",{14,672,220,34},lavender,lavender,14);ui_.End();}
UiHit ApplicationView::Hit(const ApplicationFlow& flow,double x,double y) const{
    if(flow.Screen()==AppScreen::Home){const auto rects=HomeRects();if(Contains(rects[1],x,y))return {UiAction::Practice};
        if(Contains({960,386,82,32},x,y))return {UiAction::SkinPrevious};if(Contains({1162,386,82,32},x,y))return {UiAction::SkinNext};
        if(Contains(nameBox,x,y))return {UiAction::Name};if(Contains(saveName,x,y))return {UiAction::SaveName};return {};}
    if(flow.Screen()==AppScreen::Gameplay)return Contains({14,672,220,34},x,y)?UiHit{UiAction::Back}:UiHit{};
    if(flow.Screen()==AppScreen::Result){if(Contains({645,485,247,43},x,y))return {UiAction::Back};if(!replaySaved&&Contains({388,485,247,43},x,y))return {UiAction::SaveReplay};return {};}
    if(Contains(back,x,y))return {UiAction::Back};
    if(flow.Screen()!=AppScreen::Practice)return {};
    if(sideMenu)for(int i=0;i<2;++i)if(Contains({side.x,side.y+44+i*40,side.w,40},x,y))return {UiAction::Side,std::size_t(i+1)};
    if(starMenu)for(int i=0;i<8;++i)if(Contains({stars.x,stars.y+44+i*40,stars.w,40},x,y))return {UiAction::Stars,std::size_t(i+1)};
    if(Contains(side,x,y))return {UiAction::Side};if(Contains(stars,x,y))return {UiAction::Stars};if(Contains(search,x,y))return {UiAction::Search};
    for(int i=0;i<rowCount;++i){const auto position=firstRow_+i;
        if(position<int(flow.VisibleSongs().size())&&Contains({36,rowY+i*rowHeight,1200,68},x,y))return {UiAction::Song,flow.VisibleSongs()[position]};}
    return {};
}
}
