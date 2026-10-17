#include "pke/GameSession.hpp"
#include <algorithm>
#include <cmath>
#include <exception>

namespace pke {
GameSession::GameSession(Config cfg,std::filesystem::path exe,IAudioClock* injected)
    :config_(std::move(cfg)),executableDir_(std::move(exe)),locator_(config_,executableDir_),clock_(injected?injected:&ownedAudio_),injectedClock_(injected!=nullptr){ ownedAudio_.SetDiagnostics(&diagnostics_); }
GameSession::~GameSession(){
    if(clock_) clock_->Stop();
    ownedAudio_.SetDiagnostics(nullptr);
}

void GameSession::Fail(std::string m){ failureMessage_=std::move(m); state_=SessionState::Failed; if(clock_) clock_->Stop(); }
CharacterDefinition GameSession::FallbackCharacter(const std::string& label) const {
    CharacterDefinition c; c.warnings.push_back("Using diagnostic fallback character for "+label);
    c.atlas.push_back({"fallback",{0,0,64,64},0,0,64,64,false});
    for(auto name:{"idle","singLEFT","singDOWN","singUP","singRIGHT"}){ AnimationDef a; a.anim=name;a.prefix="fallback";a.fps=1;a.loop=true;a.frameIndices={0};c.animations[name]=a; }
    return c;
}
int GameSession::LaneForKey(SDL_Scancode key) const { for(int i=0;i<4;++i) if(config_.playerKeys[i]==key) return i; return -1; }

bool GameSession::Load(const SongSelection& selection){
    state_=SessionState::Loading; failureMessage_.clear(); warnings_.clear(); nextOpponentNote_=0; lastUpdateSongTimeMs_=0; songName_=selection.songId;
    try {
        auto folder=locator_.ResolveSongFolder(selection.songId);
        auto inst=locator_.ResolveInst(folder);
        auto voices=locator_.ResolveVoices(folder);
        auto chartFile=locator_.ResolveChartFile(folder,selection.difficulty);
        chart_=ParseChart(chartFile); warnings_.insert(warnings_.end(),chart_.warnings.begin(),chart_.warnings.end()); if(!chart_.player1.empty()) songName_=selection.songId;
        rhythm_.Reset(chart_);
        try { bfDef_=LoadCharacter(locator_.ResolveCharacter(chart_.player1)); } catch(const std::exception& e){ warnings_.push_back(std::string("BF asset fallback: ")+e.what()); bfDef_=FallbackCharacter("BF"); }
        try { dadDef_=LoadCharacter(locator_.ResolveCharacter(chart_.player2)); } catch(const std::exception& e){ warnings_.push_back(std::string("DAD asset fallback: ")+e.what()); dadDef_=FallbackCharacter("DAD"); }
        bfAnimator_.SetCharacter(&bfDef_); dadAnimator_.SetCharacter(&dadDef_);
        if(!injectedClock_){ ownedAudio_.SetLatencyCompensationMs(config_.audioLatencyCompensationMs); if(!ownedAudio_.Load(inst,voices)){ Fail(ownedAudio_.LastError()); return false; } }
        if(renderer_){ if(!bfDef_.imagePath.empty()) if(!renderer_->PreloadCharacter("bf",bfDef_)) warnings_.push_back("BF texture preload failed: "+renderer_->LastError()); if(!dadDef_.imagePath.empty()) if(!renderer_->PreloadCharacter("dad",dadDef_)) warnings_.push_back("DAD texture preload failed: "+renderer_->LastError()); }
        state_=SessionState::Ready; return true;
    } catch(const std::exception& e){ Fail(e.what()); return false; }
}
void GameSession::HandleKeyDown(SDL_Scancode key){
    if(state_!=SessionState::Playing) return;
    const int lane=LaneForKey(key);
    if(lane<0) return;
    const double t=clock_->SongTimeMs()+config_.inputOffsetMs;
    const auto judgement=rhythm_.KeyDown(lane,t);
    if(!judgement) return;
    const RuntimeNoteState* hit=nullptr;
    for(const auto& n:rhythm_.NoteStates()) {
        if(n.note.lane==lane && n.judgement && std::fabs(n.hitTimeMs-t)<1e-6) hit=&n;
    }
    if(hit){
        diagnostics_.RecordInputOffset(t-hit->note.timeMs);
        const double hold=hit->note.sustainMs>0?hit->note.timeMs+hit->note.sustainMs:t+150.0;
        bfAnimator_.TriggerSing(lane,t,hold);
    }
}
void GameSession::HandleKeyUp(SDL_Scancode key){ if(state_!=SessionState::Playing)return; int lane=LaneForKey(key); if(lane>=0) rhythm_.KeyUp(lane,clock_->SongTimeMs()+config_.inputOffsetMs); }
void GameSession::UpdateOpponentAnimations(double t){
    while(nextOpponentNote_<chart_.notes.size()){
        const auto& n=chart_.notes[nextOpponentNote_]; if(n.timeMs>t) break; if(n.side==Side::Opponent){ double hold=n.sustainMs>0?n.timeMs+n.sustainMs:n.timeMs+150.0; dadAnimator_.TriggerSing(n.lane,n.timeMs,hold); } ++nextOpponentNote_;
    }
}
void GameSession::Update(){
    if(state_==SessionState::Ready){ state_=SessionState::Countdown; return; }
    if(state_==SessionState::Countdown){ if(!clock_->Start()){ Fail(clock_->LastErrorCopy().empty()?"Audio start failed":clock_->LastErrorCopy()); return; } state_=SessionState::Playing; return; }
    if(state_!=SessionState::Playing) return;
    if(clock_->State()==AudioState::DeviceLost){ failureMessage_=clock_->LastErrorCopy(); state_=SessionState::DeviceLost; return; }
    if(clock_->State()==AudioState::Error){ Fail(clock_->LastErrorCopy()); return; }
    const double t=clock_->SongTimeMs(); rhythm_.Update(t); UpdateOpponentAnimations(t); bfAnimator_.Update(t); dadAnimator_.Update(t); lastUpdateSongTimeMs_=t;
    if(clock_->State()==AudioState::Finished){ state_=SessionState::Completed; clock_->Stop(); }
}
void GameSession::RecordFrameTiming(double intervalMs,double updateMs,double renderMs) noexcept {
    diagnostics_.RecordFrame({intervalMs,updateMs,renderMs,clock_?clock_->SongTimeMs():0.0});
}
void GameSession::Render(){
    if(!renderer_) return;
    RenderState rs;
    rs.songTimeMs=clock_?clock_->SongTimeMs():0;
    rs.scrollSpeed=chart_.scrollSpeed;
    rs.chart=&chart_;
    rs.notes=&rhythm_.NoteStates();
    rs.bf={"bf",&bfDef_,bfAnimator_.CurrentFrame(),{1200,480}};
    rs.dad={"dad",&dadDef_,dadAnimator_.CurrentFrame(),{420,480}};
    rs.stats=rhythm_.Stats();
    rs.songName=songName_;
    rs.diagnosticsVisible=diagnosticsVisible_;
    if(diagnosticsVisible_) rs.diagnostics=diagnostics_.Summary();
    renderer_->Draw(rs);
}
void GameSession::RequestStop(){ if(clock_)clock_->Stop(); state_=SessionState::Completed; }
}
