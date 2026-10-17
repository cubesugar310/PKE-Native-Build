#include "pke/App.hpp"
#include "pke/ApplicationFlow.hpp"
#include "pke/ApplicationView.hpp"
#include "pke/GameSession.hpp"
#include "pke/LegacySaveImporter.hpp"
#include "pke/PopulationRuntime.hpp"
#include "pke/SaveService.hpp"
#include "pke/QuickJsRuntime.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#ifdef PKE_NATIVE_WITH_SDL
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#endif

namespace pke {
namespace {
std::int64_t UtcNow(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
std::string JsonString(std::string_view text){
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=c;}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=c;}return out+'"';
}
void EraseLast(std::string& text){if(text.empty())return;auto end=text.size()-1;while(end>0&&(static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;text.resize(end);}
}
int App::Run(int argc,char** argv){
    const auto exeDir=std::filesystem::absolute(argc>0?std::filesystem::path(argv[0]).parent_path():std::filesystem::current_path());
    std::filesystem::path localAppData;if(const char* value=std::getenv("LOCALAPPDATA"))localAppData=value;
    std::optional<std::filesystem::path> explicitLegacy;
    if(const char* value=std::getenv("PKE_LEGACY_SAVE_PATH"))if(*value)explicitLegacy=value;
    const auto migration=AutoImportLegacyFullSaves(exeDir,localAppData,explicitLegacy);
    if(migration.status==LegacyImportStatus::WriteFailed)std::cerr<<migration.message<<'\n';
    Config config;try{config=LoadConfig(exeDir,exeDir/"pke-native.json");}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
#ifndef PKE_NATIVE_WITH_SDL
    std::cout<<"PKE Native core build: SDL/audio disabled\n";return 0;
#else
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)!=0){std::cerr<<SDL_GetError()<<'\n';return 2;}
    IMG_Init(IMG_INIT_PNG|IMG_INIT_WEBP);TTF_Init();
    auto* window=SDL_CreateWindow("PKE Beat Battle",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,kLogicalWidth,kLogicalHeight,
        SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!window){std::cerr<<SDL_GetError()<<'\n';TTF_Quit();IMG_Quit();SDL_Quit();return 3;}
    auto* sdl=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|(config.vsync?SDL_RENDERER_PRESENTVSYNC:0));
    if(!sdl)sdl=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!sdl){std::cerr<<SDL_GetError()<<'\n';SDL_DestroyWindow(window);TTF_Quit();IMG_Quit();SDL_Quit();return 4;}
    int exitCode=0;
    try {
        Renderer gameplay;gameplay.Initialize(sdl);
        if(const auto* windir=std::getenv("WINDIR"))for(const auto* name:{"meiryo.ttc","segoeui.ttf"})
            if(gameplay.PreloadFont(std::filesystem::path(windir)/"Fonts"/name,19))break;
        UiRenderer ui;if(!ui.Initialize(sdl))throw std::runtime_error(ui.LastError());
        ui.Begin({21,17,34,255});ui.Text("startup","PKE Beat Battle",{200,260,880,80},40,{255,226,135,255},true);
        ui.Text("startup-status","CHECKING...",{200,360,880,50},18,{255,255,255,255},true);ui.End();
        SaveService saves(DefaultNativeSaveRoot(exeDir,localAppData));const auto loaded=saves.LoadPlayer();
        NativeSaveSummary player=loaded.player;
        if(loaded.status!=SaveLoadStatus::Loaded){player.valid=true;player.playerName="YOU";player.progress.rp=1500;player.progress.peakRp=1500;}
        const auto populationSave=saves.LoadPopulation();std::promise<std::string> catalogReady;
        const bool playerWritable=loaded.status==SaveLoadStatus::Loaded||loaded.status==SaveLoadStatus::Missing;
        const bool populationWritable=populationSave.status==SaveLoadStatus::Loaded||populationSave.status==SaveLoadStatus::Missing;
        PopulationRuntimeOptions options;options.compatibilityDirectory=exeDir/"compat";
        options.initializeJsonFuture=catalogReady.get_future().share();
        if(populationSave.status==SaveLoadStatus::Loaded)options.checkpointJson=populationSave.checkpoint.json;
        PopulationRuntime population(std::move(options));
        AssetService assets(config,exeDir,exeDir/"compat");
        try {
            assets.Refresh();auto json=assets.PopulationCatalogJson();json.pop_back();
            json+=",\"now\":"+std::to_string(UtcNow())+",\"playerName\":"+JsonString(player.playerName)+",\"playerSkin\":"+JsonString(player.selectedSkin)+'}';
            catalogReady.set_value(std::move(json));
        }catch(...){catalogReady.set_exception(std::current_exception());throw;}
        ApplicationFlow flow(assets,std::move(player));ApplicationView view(ui);view.SetWallClock(UtcNow());view.Prepare(flow);
        if(!playerWritable)view.persistenceError="Player save preserved; writes disabled: "+loaded.error;
        if(!populationWritable)view.persistenceError+=" Population save preserved; writes disabled: "+populationSave.error;
        std::unique_ptr<GameSession> session;
        std::future<PopulationCheckpoint> pendingCheckpoint;std::optional<PopulationCheckpoint> lastCheckpoint;
        auto nextCheckpoint=UtcNow()+30000;
        // Native time continues even through a modal OS operation or a resize.
        std::jthread clockPump([&](std::stop_token stop){while(!stop.stop_requested()){
            population.AdvanceTo(UtcNow());std::this_thread::sleep_for(std::chrono::milliseconds(100));}});
        const auto savePlayer=[&]{if(!playerWritable)return;const auto result=saves.SavePlayer(flow.Player());if(!result.ok)view.status=result.error;};
        const auto persistPlayer=[&](const NativeSaveSummary& candidate){
            if(!playerWritable){view.status="Player save is preserved; writes disabled.";return false;}
            const auto result=saves.SavePlayer(candidate);if(!result.ok)view.persistenceError=result.error;return result.ok;
        };
        const auto requestCheckpoint=[&]{if(populationWritable&&!pendingCheckpoint.valid())pendingCheckpoint=population.RequestCheckpoint();};
        const auto back=[&]{if(session){session->RequestStop();session.reset();}flow.Back();view.sideMenu=view.starMenu=view.searchFocus=view.nameFocus=false;
            view.composition.clear();view.developerInput.clear();view.developerCode.clear();view.developerPinStage=false;
            SDL_StopTextInput();view.status.clear();view.EnsureSelected(flow);requestCheckpoint();};
        const auto launch=[&]{if(!flow.SelectedSong())return;
            auto candidate=std::make_unique<GameSession>(config,exeDir);candidate->SetRenderer(&gameplay);
            candidate->ApplyPreferences(flow.Player());
            if(!candidate->LoadPractice(flow.Catalog()->songs.at(*flow.SelectedSong()),*flow.Catalog(),flow.PracticeSide(),flow.Player().selectedSkin,false)){
                view.status=candidate->FailureMessage();return;}
            if(!flow.BeginGameplay())return;session=std::move(candidate);view.replaySaved=false;view.searchFocus=view.nameFocus=false;view.sideMenu=view.starMenu=false;
            view.status.clear();view.composition.clear();SDL_StopTextInput();};
        const auto commitName=[&]{flow.SetPlayerName(view.nameEdit);savePlayer();view.nameFocus=false;view.composition.clear();SDL_StopTextInput();};
        const auto changeSkin=[&](int direction){std::vector<std::string> characters;
            for(const auto& c:flow.Catalog()->characters)if(flow.SkinAvailable(c.id))characters.push_back(c.sourceSkinId.empty()?c.id:c.sourceSkinId);if(characters.empty())return;
            auto found=std::find(characters.begin(),characters.end(),flow.Player().selectedSkin);
            int position=found==characters.end()?0:int(std::distance(characters.begin(),found));
            position=(position+direction+int(characters.size()))%int(characters.size());flow.SetSkin(characters[position]);savePlayer();view.Prepare(flow);};
        const auto authenticate=[&]{
            if(!view.developerPinStage){view.developerCode=std::move(view.developerInput);view.developerInput.clear();view.developerPinStage=true;return;}
            const bool enabled=flow.EnableDeveloper(view.developerCode,view.developerInput);view.developerInput.clear();view.developerCode.clear();view.developerPinStage=false;
            view.status=enabled?"Developer mode enabled":"Invalid code or PIN";if(enabled)SDL_StopTextInput();
        };
        bool running=true,fullscreen=false;auto previous=std::chrono::steady_clock::now();const auto opened=previous;
        while(running){
            const auto frameStart=std::chrono::steady_clock::now();const double interval=std::chrono::duration<double,std::milli>(frameStart-previous).count();previous=frameStart;
            SDL_Event event{};
            while(SDL_PollEvent(&event)){
                if(event.type==SDL_QUIT){running=false;continue;}
                if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_FOCUS_LOST&&session&&flow.Screen()==AppScreen::Gameplay)
                    for(auto key:config.playerKeys)session->HandleKeyUp(key);
                if(event.type==SDL_TEXTEDITING){view.composition=event.edit.text;continue;}
                if(event.type==SDL_TEXTINPUT){view.composition.clear();if(view.searchFocus){flow.SetSearch(flow.Search()+event.text.text);view.EnsureSelected(flow);}
                    else if(view.nameFocus)view.nameEdit+=event.text.text;
                    else if(flow.Screen()==AppScreen::Developer&&!flow.DeveloperEnabled()&&view.developerInput.size()<128)view.developerInput+=event.text.text;continue;}
                if(event.type==SDL_MOUSEWHEEL&&flow.Screen()==AppScreen::Practice){view.Scroll(flow,-event.wheel.y);continue;}
                if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_LEFT){
                    int ww,wh,ow,oh;SDL_GetWindowSize(window,&ww,&wh);SDL_GetRendererOutputSize(sdl,&ow,&oh);
                    const auto point=WindowToLogical(event.button.x,event.button.y,ww,wh,ow,oh);if(!point)continue;
                    view.SetWallClock(UtcNow());const auto hit=view.Hit(flow,point->x,point->y);
                    if(hit.action!=UiAction::Name&&hit.action!=UiAction::SaveName&&view.nameFocus)commitName();
                    if(hit.action!=UiAction::Search){view.searchFocus=false;view.composition.clear();if(!view.nameFocus&&!(flow.Screen()==AppScreen::Developer&&!flow.DeveloperEnabled()))SDL_StopTextInput();}
                    switch(hit.action){
                        case UiAction::Practice:flow.OpenPractice();view.status.clear();view.EnsureSelected(flow);break;
                        case UiAction::Back:back();break;
                        case UiAction::Halloween:flow.OpenHalloween();view.historyTab=false;view.status.clear();view.Prepare(flow);break;
                        case UiAction::ShopTab:view.historyTab=false;break;
                        case UiAction::HistoryTab:view.historyTab=true;break;
                        case UiAction::BuyTitle:view.status="Purchase: "+flow.PurchaseHalloweenTitle(UtcNow(),persistPlayer);break;
                        case UiAction::BuySkin:view.status="Purchase: "+flow.PurchaseHalloweenSkin(hit.index,UtcNow(),persistPlayer);break;
                        case UiAction::Settings:flow.OpenSettings();view.status.clear();break;
                        case UiAction::VolumeDown:case UiAction::VolumeUp:case UiAction::ScrollDirection:case UiAction::JudgementPosition:{
                            const auto& p=flow.Player();const double volume=std::clamp(p.masterVolume+(hit.action==UiAction::VolumeDown?-0.1:hit.action==UiAction::VolumeUp?0.1:0.0),0.0,1.0);
                            const auto scroll=hit.action==UiAction::ScrollDirection?(p.scrollDirection=="up"?"down":"up"):p.scrollDirection;
                            const auto judgement=hit.action==UiAction::JudgementPosition?(p.judgementPosition=="center"?"corner":"center"):p.judgementPosition;
                            view.status=flow.SavePreferences(volume,scroll,judgement,persistPlayer)?"Saved":"Save failed";break;}
                        case UiAction::Developer:flow.OpenDeveloper();view.status.clear();view.developerInput.clear();view.developerCode.clear();view.developerPinStage=false;if(!flow.DeveloperEnabled())SDL_StartTextInput();break;
                        case UiAction::DeveloperAuthenticate:authenticate();break;
                        case UiAction::EventOverride:{const std::array<const char*,3> modes{"auto","force-on","force-off"};
                            view.status=flow.SaveEventOverride(modes.at(hit.index),persistPlayer)?"Saved":"Save failed";break;}
                        case UiAction::Song:flow.SelectSong(hit.index);launch();break;
                        case UiAction::Search:view.searchFocus=true;view.nameFocus=false;SDL_StartTextInput();break;
                        case UiAction::Name:view.nameEdit=flow.Player().playerName;view.nameFocus=true;SDL_StartTextInput();break;
                        case UiAction::SaveName:if(view.nameFocus)commitName();break;
                        case UiAction::SkinPrevious:changeSkin(-1);break;
                        case UiAction::SkinNext:changeSkin(1);break;
                        case UiAction::SaveReplay:if(session&&!view.replaySaved){try{
                            const auto json=session->ExportReplay(exeDir/"compat");
                            const auto destination=DefaultNativeSaveRoot(exeDir,localAppData)/"replays"/("practice-"+std::to_string(UtcNow())+".pke-replay.json");
                            WriteValidatedSaveFile(destination,json,[](std::string_view text){QuickJsRuntime check;
                                const auto parsed=check.Evaluate("JSON.parse("+JsonString(text)+")");if(!parsed.ok)throw std::runtime_error(parsed.error);});
                            view.replaySaved=true;view.status=destination.string();
                        }catch(const std::exception& error){view.status=error.what();}}break;
                        case UiAction::Side:if(hit.index){flow.SetSide(hit.index==1?Side::Player:Side::Opponent);view.sideMenu=false;view.EnsureSelected(flow);}else{view.sideMenu=!view.sideMenu;view.starMenu=false;}break;
                        case UiAction::Stars:if(hit.index){flow.SetStars(int(hit.index)-1);view.starMenu=false;view.EnsureSelected(flow);}else{view.starMenu=!view.starMenu;view.sideMenu=false;}break;
                        default:view.sideMenu=view.starMenu=false;break;
                    }
                }
                if(event.type==SDL_KEYDOWN){const auto key=event.key.keysym.scancode;
                    if(key==SDL_SCANCODE_F11&&!event.key.repeat){fullscreen=!fullscreen;SDL_SetWindowFullscreen(window,fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);continue;}
                    if(flow.Screen()==AppScreen::Developer&&!flow.DeveloperEnabled()){
                        if(key==SDL_SCANCODE_ESCAPE)back();else if(key==SDL_SCANCODE_RETURN&&!event.key.repeat)authenticate();else if(key==SDL_SCANCODE_BACKSPACE)EraseLast(view.developerInput);continue;}
                    if(view.nameFocus){if(key==SDL_SCANCODE_RETURN)commitName();else if(key==SDL_SCANCODE_ESCAPE){view.nameFocus=false;SDL_StopTextInput();}
                        else if(key==SDL_SCANCODE_BACKSPACE)EraseLast(view.nameEdit);continue;}
                    if(view.searchFocus){if(key==SDL_SCANCODE_BACKSPACE){auto text=flow.Search();EraseLast(text);flow.SetSearch(std::move(text));view.EnsureSelected(flow);}
                        else if(key==SDL_SCANCODE_ESCAPE||key==SDL_SCANCODE_TAB){view.searchFocus=false;view.composition.clear();SDL_StopTextInput();}continue;}
                    if(key==SDL_SCANCODE_ESCAPE&&!event.key.repeat){if(view.sideMenu||view.starMenu)view.sideMenu=view.starMenu=false;
                        else if(flow.Screen()!=AppScreen::Home)back();continue;}
                    if(flow.Screen()==AppScreen::Practice){if(key==SDL_SCANCODE_UP||key==SDL_SCANCODE_DOWN){flow.MoveSelection(key==SDL_SCANCODE_UP?-1:1);view.EnsureSelected(flow);}
                        else if(key==SDL_SCANCODE_RETURN&&!event.key.repeat)launch();else if(key==SDL_SCANCODE_TAB){view.searchFocus=true;SDL_StartTextInput();}}
                    else if(flow.Screen()==AppScreen::Home&&key==SDL_SCANCODE_RETURN&&!event.key.repeat){flow.OpenPractice();view.EnsureSelected(flow);}
                    else if(flow.Screen()==AppScreen::Result&&key==SDL_SCANCODE_RETURN&&!event.key.repeat)back();
                    else if(flow.Screen()==AppScreen::Gameplay&&session&&!event.key.repeat){if(key==SDL_SCANCODE_F3)session->ToggleDiagnostics();else session->HandleKeyDown(key);}
                }
                if(event.type==SDL_KEYUP&&flow.Screen()==AppScreen::Gameplay&&session)session->HandleKeyUp(event.key.keysym.scancode);
            }
            if(!running)break;
            if(flow.Screen()==AppScreen::Gameplay&&session){const auto updateStart=std::chrono::steady_clock::now();session->Update();const auto drawStart=std::chrono::steady_clock::now();
                session->Render(false);view.GameplayOverlay(flow.Player().scrollDirection=="down");const auto drawEnd=std::chrono::steady_clock::now();
                session->RecordFrameTiming(interval,std::chrono::duration<double,std::milli>(drawStart-updateStart).count(),std::chrono::duration<double,std::milli>(drawEnd-drawStart).count());
                if(session->State()==SessionState::Completed){flow.FinishGameplay(session->Stats());view.status.clear();requestCheckpoint();}
                else if(session->State()==SessionState::Failed||session->State()==SessionState::DeviceLost){const auto error=session->FailureMessage();back();view.status=error;}
            }else{
                if(pendingCheckpoint.valid()&&pendingCheckpoint.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
                    auto checkpoint=pendingCheckpoint.get();if(checkpoint.ok){const auto result=saves.SavePopulation(checkpoint);
                        if(result.ok)lastCheckpoint=std::move(checkpoint);else view.status=result.error;}else view.status=checkpoint.error;}
                const auto now=UtcNow();if(now>=nextCheckpoint){requestCheckpoint();nextCheckpoint=now+30000;}
                const auto snapshot=population.Snapshot();const auto state=population.Status();
                view.SetWallClock(now);view.Draw(flow,snapshot.get(),state.get(),std::chrono::duration<double,std::milli>(frameStart-opened).count());
            }
            if(!config.vsync&&config.fpsCap>0){const auto remaining=std::chrono::duration<double>(1.0/config.fpsCap)-(std::chrono::steady_clock::now()-frameStart);
                if(remaining.count()>0)std::this_thread::sleep_for(remaining);}
        }
        if(view.nameFocus)commitName();clockPump.request_stop();clockPump.join();population.AdvanceTo(UtcNow());
        if(populationWritable){auto finalCheckpoint=population.RequestCheckpoint();if(finalCheckpoint.wait_for(std::chrono::seconds(10))==std::future_status::ready){
            const auto checkpoint=finalCheckpoint.get();if(checkpoint.ok){const auto result=saves.SavePopulation(checkpoint);if(!result.ok)std::cerr<<result.error<<'\n';}
            else if(lastCheckpoint)saves.SavePopulation(*lastCheckpoint);
        }else if(lastCheckpoint)saves.SavePopulation(*lastCheckpoint);}
        population.Stop();session.reset();
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"PKE Beat Battle",error.what(),window);exitCode=5;}
    SDL_DestroyRenderer(sdl);SDL_DestroyWindow(window);TTF_Quit();IMG_Quit();SDL_Quit();return exitCode;
#endif
}
}
