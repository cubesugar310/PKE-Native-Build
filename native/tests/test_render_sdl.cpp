#include "TestHarness.hpp"
#include "pke/Renderer.hpp"
#include "pke/UiRenderer.hpp"
#include "pke/ApplicationView.hpp"
#include "pke/GameSession.hpp"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <chrono>
#include <cstdlib>
#include <fstream>

namespace {
struct SurfaceRenderer {
    SDL_Surface* surface{};SDL_Renderer* renderer{};
    SurfaceRenderer(int w,int h){surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);
        if(!surface)throw std::runtime_error(SDL_GetError());renderer=SDL_CreateSoftwareRenderer(surface);
        if(!renderer)throw std::runtime_error(SDL_GetError());}
    ~SurfaceRenderer(){SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);}
    Uint32 Pixel(int x,int y){const auto* row=static_cast<const unsigned char*>(surface->pixels)+y*surface->pitch;return reinterpret_cast<const Uint32*>(row)[x];}
    void Color(int x,int y,int r,int g,int b){Uint8 red,green,blue,alpha;SDL_GetRGBA(Pixel(x,y),surface->format,&red,&green,&blue,&alpha);
        PKE_REQUIRE(red==r);PKE_REQUIRE(green==g);PKE_REQUIRE(blue==b);}
};
std::filesystem::path Font(){
#ifdef _WIN32
    const char* windows=std::getenv("WINDIR");if(windows)return std::filesystem::path(windows)/"Fonts"/"segoeui.ttf";
#endif
    for(const auto* path:{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/opentype/urw-base35/NimbusSans-Regular.otf"})
        if(std::filesystem::is_regular_file(path))return path;
    throw std::runtime_error("No system font for SDL text-cache validation");
}
}

PKE_TEST("SDL renderer clears real letterbox pixels and draws inside the logical canvas") {
    for(const auto [w,h]:{std::pair{1280,1024},std::pair{2560,1080},std::pair{1920,1080},std::pair{1001,720}}){
        SurfaceRenderer output(w,h);pke::Renderer renderer;PKE_REQUIRE(renderer.Initialize(output.renderer));
        renderer.Draw({});const auto vp=pke::ComputeLetterboxedViewport(w,h);
        output.Color(w/2,h/2,20,20,28);
        if(vp.destination.y){output.Color(w/2,0,0,0,0);output.Color(w/2,h-1,0,0,0);
            output.Color(w/2,vp.destination.y+vp.destination.h,0,0,0);}
        if(vp.destination.x){output.Color(0,h/2,0,0,0);output.Color(w-1,h/2,0,0,0);}
        renderer.Draw({});output.Color(w/2,h/2,20,20,28);
    }
}

PKE_TEST("SDL text cache reuses unchanged glyphs and refreshes after font size changes") {
    SurfaceRenderer output(1280,720);pke::Renderer renderer;PKE_REQUIRE(renderer.Initialize(output.renderer));
    PKE_REQUIRE(renderer.PreloadFont(Font(),20));pke::RenderState state;state.songName="Cached song";
    renderer.Draw(state);const auto initial=renderer.CacheStats();PKE_REQUIRE(initial.textRasterizations==2);
    for(int i=0;i<10;++i)renderer.Draw(state);
    PKE_REQUIRE(renderer.CacheStats().textRasterizations==initial.textRasterizations);
    PKE_REQUIRE(renderer.CacheStats().textCacheHits>=20);
    PKE_REQUIRE(renderer.PreloadFont(Font(),30));renderer.Draw(state);
    PKE_REQUIRE(renderer.CacheStats().textRasterizations==initial.textRasterizations+2);
    PKE_REQUIRE(!renderer.PreloadFont("/missing-font.ttf",20));renderer.Draw(state);
    PKE_REQUIRE(renderer.CacheStats().textRasterizations==initial.textRasterizations+2);
}

PKE_TEST("SDL character texture cache retains the loaded image when its source disappears") {
    SurfaceRenderer output(1280,720);pke::Renderer renderer;PKE_REQUIRE(renderer.Initialize(output.renderer));
    const auto path=std::filesystem::temp_directory_path()/("pke-texture-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bmp");
    PKE_REQUIRE(SDL_SaveBMP(output.surface,path.string().c_str())==0);
    pke::CharacterDefinition definition;definition.imagePath=path;
    PKE_REQUIRE(renderer.PreloadCharacter("bf",definition));std::filesystem::remove(path);
    PKE_REQUIRE(renderer.PreloadCharacter("bf",definition));PKE_REQUIRE(renderer.CacheStats().imageLoads==1);
    PKE_REQUIRE(!renderer.PreloadCharacter("new-key",definition));
}
PKE_TEST("Rotated atlas pixels retain the original unrotated size and horizontal mirror") {
    SurfaceRenderer source(4,2);const auto path=std::filesystem::temp_directory_path()/("pke-rotated-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bmp");
    for(int y=0;y<2;++y)for(int x=0;x<4;++x){SDL_Rect pixel{x,y,1,1};
        const auto color=y==0?(x<2?SDL_Color{255,0,0,255}:SDL_Color{0,255,0,255}):(x<2?SDL_Color{0,0,255,255}:SDL_Color{255,255,0,255});
        SDL_FillRect(source.surface,&pixel,SDL_MapRGB(source.surface->format,color.r,color.g,color.b));}
    PKE_REQUIRE(SDL_SaveBMP(source.surface,path.string().c_str())==0);
    SurfaceRenderer output(1280,720);pke::Renderer renderer;PKE_REQUIRE(renderer.Initialize(output.renderer));
    pke::CharacterDefinition c;c.imagePath=path;c.scale=20;c.flipX=true;c.noAntialiasing=true;
    c.atlas.push_back({"idle",{0,0,4,2},0,0,2,4,true});PKE_REQUIRE(renderer.PreloadCharacter("bf",c));std::filesystem::remove(path);
    pke::RenderState state;state.bf={"bf",&c,{0,"idle",0},{100,200}};renderer.Draw(state);
    output.Color(110,210,255,255,0);output.Color(130,210,0,255,0);
    output.Color(110,270,0,0,255);output.Color(130,270,255,0,0);
}
PKE_TEST("Practice renderer uses the original anchors and applies saved transforms before drawing") {
    const auto root=std::filesystem::temp_directory_path()/("pke-source-pose-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::filesystem::remove_all(path);}} cleanup{root};
    std::filesystem::create_directories(root/"compat");std::filesystem::copy_file(std::filesystem::path(PKE_COMPAT_DIR)/"character-presentation.js",root/"compat/character-presentation.js");
    SurfaceRenderer pixels(12,12);SDL_FillRect(pixels.surface,nullptr,SDL_MapRGB(pixels.surface->format,255,0,0));
    const auto path=root/"bf.bmp";PKE_REQUIRE(SDL_SaveBMP(pixels.surface,path.string().c_str())==0);
    auto definition=std::make_shared<pke::CharacterDefinition>();definition->imagePath=path;
    definition->atlas.push_back({"idle",{0,0,12,12},0,0,12,12,false});pke::AnimationDef idle;idle.anim="idle";idle.frameIndices={0};definition->animations["idle"]=idle;pke::PrepareCharacterGeometry(*definition);
    pke::AssetCatalog catalog;catalog.characters.push_back({"bf","BF",{},definition,"sparrow:bf"});
    pke::SongAsset song;song.displayName="Pose";song.charts.emplace_back();song.charts[0].chart.player1="bf";
    class Clock final:public pke::IAudioClock{public:bool Start()override{return true;}void Pause()override{}void Resume()override{}void Stop()override{}double SongTimeMs()const override{return 0;}pke::AudioState State()const override{return pke::AudioState::Ready;}std::string LastErrorCopy()const override{return {};}} clock;
    SurfaceRenderer output(1280,720);pke::Renderer renderer;PKE_REQUIRE(renderer.Initialize(output.renderer));
    {pke::GameSession session({},root,&clock);session.SetRenderer(&renderer);PKE_REQUIRE(session.LoadPractice(song,catalog,pke::Side::Player,"sparrow:bf",false));session.Render();
        output.Color(1082,683,255,0,0);output.Color(800,320,20,20,28);}
    {pke::GameSession session({},root,&clock);session.SetRenderer(&renderer);pke::NativeSaveSummary prefs;
        prefs.characterLayoutsJson=R"({"bf":{"psych:bf":{"scale":0.5,"offsetX":-400,"offsetY":-300,"flipXOverride":false}}})";
        session.ApplyPreferences(prefs);PKE_REQUIRE(session.LoadPractice(song,catalog,pke::Side::Player,"sparrow:bf",false));
        std::filesystem::remove(root/"compat/character-presentation.js");std::filesystem::remove(path);session.Render();
        output.Color(687,390,255,0,0);output.Color(1082,683,20,20,28);}
}

PKE_TEST("Gameplay uses source lane centers and colored directional notes at the audio-clock position") {
    SurfaceRenderer output(1280,720);pke::Renderer renderer;PKE_REQUIRE(renderer.Initialize(output.renderer));
    pke::Chart chart;chart.notes.push_back({1000,0,0,pke::Side::Player});pke::RhythmEngine rhythm;rhythm.Reset(chart);
    pke::RenderState state;state.chart=&chart;state.notes=&rhythm.NoteStates();renderer.Draw(state);
    output.Color(776,94,100,100,110);output.Color(98,94,100,100,110);
    output.Color(776,544,184,74,152);
}
PKE_TEST("Downscroll mirrors receptor and sustain geometry while preserving lane ownership") {
    SurfaceRenderer output(1280,720);pke::Renderer renderer;PKE_REQUIRE(renderer.Initialize(output.renderer));
    pke::Chart chart;chart.notes.push_back({1000,0,200,pke::Side::Player});pke::RhythmEngine rhythm;rhythm.Reset(chart);
    pke::RenderState state;state.chart=&chart;state.notes=&rhythm.NoteStates();state.downScroll=true;renderer.Draw(state);
    output.Color(776,626,100,100,110);output.Color(98,626,100,100,110);
    output.Color(776,176,184,74,152);output.Color(776,120,184,74,152);
    output.Color(776,544,20,20,28);
    output.Color(400,70,80,220,120);output.Color(400,660,20,20,28);
}

PKE_TEST("UI text uses preloaded font bytes and unchanged labels reuse their textures") {
    SurfaceRenderer output(1280,720);pke::UiRenderer ui;
    const auto font=std::filesystem::temp_directory_path()/("pke-ui-font-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+Font().extension().string());
    std::filesystem::copy_file(Font(),font);PKE_REQUIRE(ui.Initialize(output.renderer,font));std::filesystem::remove(font);
    for(int i=0;i<3;++i){ui.Begin({21,17,34,255});ui.Panel("panel",{100,100,200,80},{41,32,54,255},{41,32,54,255},14);
        PKE_REQUIRE(ui.Text("label","Cached UI",{100,100,200,80},20,{255,255,255,255},true));ui.End();}
    PKE_REQUIRE(ui.CacheStats().textRasterizations==1);
    PKE_REQUIRE(ui.Text("label","New size",{100,100,200,80},30,{255,255,255,255},true));
    PKE_REQUIRE(ui.CacheStats().textRasterizations==2);output.Color(100,100,21,17,34);
}

PKE_TEST("Native scene pixels and hit targets traverse the complete Practice route") {
    const auto root=std::filesystem::temp_directory_path()/("pke-render-flow-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::filesystem::remove_all(p);}} cleanup{root};
    const auto folder=root/"Songs"/"Demo";std::filesystem::create_directories(folder);
    std::ofstream(folder/"Inst.ogg")<<"clock-injected";
    std::ofstream(folder/"demo-hard.json")<<R"({"song":{"song":"Demo Song","bpm":120,"speed":1.8,"notes":[{"mustHitSection":true,"sectionNotes":[[1000,0,1000],[2200,2,0]]}]}})";
    pke::Config config;config.songsRoot="Songs";config.charactersRoot="Characters";
    pke::AssetService assets(config,root,PKE_COMPAT_DIR,"en-US");assets.Refresh();
    pke::NativeSaveSummary player;player.valid=true;player.playerName="YOU";player.progress.rp=1500;player.progress.wins=12;
    pke::ApplicationFlow flow(assets,player);SurfaceRenderer output(1280,720);pke::UiRenderer ui;
    PKE_REQUIRE(ui.Initialize(output.renderer));pke::ApplicationView view(ui);view.Prepare(flow);
    const auto capture=[&](const char* name){if(const auto* directory=std::getenv("PKE_TEST_CAPTURE_DIR")){
        std::filesystem::create_directories(directory);PKE_REQUIRE(IMG_SavePNG(output.surface,(std::filesystem::path(directory)/(std::string(name)+".png")).string().c_str())==0);}};
    view.Draw(flow,nullptr,nullptr,0);capture("home");output.Color(0,0,92,0,8);
    PKE_REQUIRE(view.Hit(flow,250,580).action==pke::UiAction::Practice);
    flow.OpenPractice();view.EnsureSelected(flow);view.Draw(flow,nullptr,nullptr,0);capture("practice");
    PKE_REQUIRE(view.Hit(flow,500,130).action==pke::UiAction::Search);
    view.sideMenu=true;PKE_REQUIRE(view.Hit(flow,200,210).index==2);view.sideMenu=false;
    const auto song=view.Hit(flow,500,250);PKE_REQUIRE(song.action==pke::UiAction::Song);flow.SelectSong(song.index);
    class Clock final:public pke::IAudioClock{public:double time{};pke::AudioState state{pke::AudioState::Ready};
        bool Start()override{state=pke::AudioState::Playing;return true;}void Pause()override{}void Resume()override{}
        void Stop()override{state=pke::AudioState::Stopped;}double SongTimeMs()const override{return time;}
        pke::AudioState State()const override{return state;}std::string LastErrorCopy()const override{return {};}} clock;
    pke::Renderer game;PKE_REQUIRE(game.Initialize(output.renderer));PKE_REQUIRE(game.PreloadFont(Font(),20));
    pke::GameSession session(config,root,&clock);session.SetRenderer(&game);
    PKE_REQUIRE(session.LoadPractice(flow.Catalog()->songs[song.index],*flow.Catalog(),pke::Side::Player,""));
    PKE_REQUIRE(flow.BeginGameplay());session.Update();session.Update();session.Render(false);view.GameplayOverlay();capture("gameplay");
    clock.time=1000;session.HandleKeyDown(SDL_SCANCODE_LEFT);clock.time=1400;session.Update();session.Render(false);view.GameplayOverlay();capture("sustain");
    clock.time=2000;session.Update();clock.time=2200;session.HandleKeyDown(SDL_SCANCODE_UP);clock.state=pke::AudioState::Finished;session.Update();
    PKE_REQUIRE(session.State()==pke::SessionState::Completed);flow.FinishGameplay(session.Stats());view.Draw(flow,nullptr,nullptr,0);capture("result");
    PKE_REQUIRE(flow.Result().stats.sick==2);PKE_REQUIRE(view.Hit(flow,500,500).action==pke::UiAction::SaveReplay);
    view.replaySaved=true;PKE_REQUIRE(view.Hit(flow,500,500).action==pke::UiAction::None);
    PKE_REQUIRE(view.Hit(flow,750,500).action==pke::UiAction::Back);flow.Back();flow.Back();
    PKE_REQUIRE(flow.Screen()==pke::AppScreen::Home);PKE_REQUIRE(flow.Player().progress.rp==1500);PKE_REQUIRE(flow.Player().progress.wins==12);
    view.SetWallClock(1792000000000LL);view.Draw(flow,nullptr,nullptr,0);capture("home-halloween");
    PKE_REQUIRE(view.Hit(flow,100,124).action==pke::UiAction::Halloween);flow.OpenHalloween();view.Prepare(flow);
    view.Draw(flow,nullptr,nullptr,0);capture("halloween-shop");
    PKE_REQUIRE(view.Hit(flow,100,532).action==pke::UiAction::None);
    PKE_REQUIRE(view.Hit(flow,320,160).action==pke::UiAction::HistoryTab);view.historyTab=true;
    view.Draw(flow,nullptr,nullptr,0);capture("halloween-history");flow.Back();flow.OpenSettings();
    view.Draw(flow,nullptr,nullptr,0);capture("settings");PKE_REQUIRE(view.Hit(flow,750,300).action==pke::UiAction::ScrollDirection);
    flow.Back();flow.OpenDeveloper();view.Draw(flow,nullptr,nullptr,0);capture("developer");
}
