#include "pke/GameSession.hpp"
#include "pke/QuickJsRuntime.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace pke {
GameSession::GameSession(Config cfg,std::filesystem::path exe,IAudioClock* injected)
    :config_(std::move(cfg)),executableDir_(std::move(exe)),locator_(config_,executableDir_),clock_(injected?injected:&ownedAudio_),injectedClock_(injected!=nullptr){ ownedAudio_.SetDiagnostics(&diagnostics_); }
GameSession::~GameSession(){
    if(clock_) clock_->Stop();
    ownedAudio_.SetDiagnostics(nullptr);
}
void GameSession::ApplyPreferences(const NativeSaveSummary& player){
    ownedAudio_.SetMasterVolume(player.masterVolume);downScroll_=player.scrollDirection=="down";cornerJudgement_=player.judgementPosition=="corner";
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
bool GameSession::LoadPractice(const SongAsset& song,const AssetCatalog& catalog,Side side,std::string_view skin,bool allowChartSkinFallback){
    state_=SessionState::Loading;failureMessage_.clear();warnings_.clear();nextOpponentNote_=0;
    songName_=song.displayName;judgementText_.clear();lastJudgementAt_=-1000;
    songFolder_=song.folderName;instName_=song.instPath.filename().string();
    voicesName_=song.voicesPath?song.voicesPath->filename().string():"";
    replayInputs_.clear();practiceOpponent_=side==Side::Opponent;
    try{
        if(song.charts.empty())throw std::runtime_error("No cached chart");
        chart_=song.charts.front().chart;
        if(side==Side::Opponent)for(auto& note:chart_.notes)note.side=note.side==Side::Player?Side::Opponent:Side::Player;
        rhythm_.Reset(chart_);
        auto character=[&](std::string id){
            if(const auto* c=FindCharacterAsset(catalog,id))return *c->definition;
            return FallbackCharacter(id);
        };
        const auto playerId=skin.empty()?(allowChartSkinFallback?(chart_.player1.empty()?"bf":chart_.player1):""):std::string(skin);
        if(playerId.empty())warnings_.push_back("No unlocked player skin available; using diagnostic fallback");
        bfDef_=character(playerId);dadDef_=character(song.opponentCharacterId.empty()?chart_.player2:song.opponentCharacterId);
        bfAnimator_.SetCharacter(&bfDef_);dadAnimator_.SetCharacter(&dadDef_);
        if(!injectedClock_){ownedAudio_.SetLatencyCompensationMs(config_.audioLatencyCompensationMs);
            if(!ownedAudio_.Load(song.instPath,song.voicesPath)){Fail(ownedAudio_.LastError());return false;}}
        if(renderer_){if(!bfDef_.imagePath.empty())renderer_->PreloadCharacter("bf",bfDef_);
            if(!dadDef_.imagePath.empty())renderer_->PreloadCharacter("dad",dadDef_);}
        state_=SessionState::Ready;return true;
    }catch(const std::exception& e){Fail(e.what());return false;}
}
void GameSession::HandleKeyDown(SDL_Scancode key){
    if(state_!=SessionState::Playing) return;
    const int lane=LaneForKey(key);
    if(lane<0) return;
    replayInputs_.push_back({std::round(clock_->SongTimeMs()*1000)/1000,lane,false});
    const double t=clock_->SongTimeMs()+config_.inputOffsetMs;
    const auto judgement=rhythm_.KeyDown(lane,t,clock_->SongTimeMs());
    if(!judgement) return;
    judgementText_=*judgement==Judgement::Sick?"SICK!":*judgement==Judgement::Good?"GOOD":"BAD";lastJudgementAt_=t;
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
void GameSession::HandleKeyUp(SDL_Scancode key){ if(state_!=SessionState::Playing)return; int lane=LaneForKey(key); if(lane>=0){
    replayInputs_.push_back({std::round(clock_->SongTimeMs()*1000)/1000,lane,true});
    rhythm_.KeyUp(lane,clock_->SongTimeMs());}}
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
    const double t=clock_->SongTimeMs(); rhythm_.Update(t,t+config_.inputOffsetMs); UpdateOpponentAnimations(t); bfAnimator_.Update(t); dadAnimator_.Update(t); lastUpdateSongTimeMs_=t;
    if(clock_->State()==AudioState::Finished){ state_=SessionState::Completed; clock_->Stop(); }
}
void GameSession::RecordFrameTiming(double intervalMs,double updateMs,double renderMs) noexcept {
    diagnostics_.RecordFrame({intervalMs,updateMs,renderMs,clock_?clock_->SongTimeMs():0.0});
}
void GameSession::Render(bool present){
    if(!renderer_) return;
    RenderState rs;
    rs.songTimeMs=clock_?clock_->SongTimeMs():0;
    rs.scrollSpeed=chart_.scrollSpeed;
    rs.downScroll=downScroll_;rs.cornerJudgement=cornerJudgement_;
    rs.chart=&chart_;
    rs.notes=&rhythm_.NoteStates();
    rs.bf={"bf",&bfDef_,bfAnimator_.CurrentFrame(),{800,320},2.0/3.0};
    rs.dad={"dad",&dadDef_,dadAnimator_.CurrentFrame(),{280,320},2.0/3.0};
    rs.stats=rhythm_.Stats();
    rs.songName=songName_;
    if(rs.songTimeMs-lastJudgementAt_<600)rs.judgementText=judgementText_;
    rs.diagnosticsVisible=diagnosticsVisible_;
    if(diagnosticsVisible_) rs.diagnostics=diagnostics_.Summary();
    renderer_->Draw(rs,present);
}
void GameSession::RequestStop(){ if(clock_)clock_->Stop(); state_=SessionState::Completed; }

std::string GameSession::ExportReplay(const std::filesystem::path& compatibilityDirectory) const{
    if(state_!=SessionState::Completed)throw std::runtime_error("Complete the song before saving its replay");
    std::ifstream script(compatibilityDirectory/"practice-replay.js",std::ios::binary);
    if(!script)throw std::runtime_error("Missing Practice replay compatibility script");
    const std::string source{std::istreambuf_iterator<char>(script),{}};
    QuickJsRuntime runtime;const auto loaded=runtime.Evaluate(source,"practice-replay.js");
    if(!loaded.ok)throw std::runtime_error(loaded.error);
    const auto quote=[](std::string_view value){std::string out="\"";constexpr char hex[]="0123456789abcdef";
        for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=c;}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=c;}return out+'"';};
    std::ostringstream json;json<<std::setprecision(17);
    json<<"{\"song\":"<<quote(songName_)<<",\"songFolder\":"<<quote(songFolder_)<<",\"opponent\":"<<quote(chart_.player2)
        <<",\"inst\":"<<quote(instName_)<<",\"voices\":"<<quote(voicesName_)<<",\"side\":"<<(practiceOpponent_?"true":"false")
        <<",\"offset\":"<<config_.inputOffsetMs<<",\"speed\":"<<chart_.scrollSpeed<<",\"notes\":[";
    bool comma=false;for(const auto& n:chart_.notes){if(comma)json<<',';comma=true;
        json<<"{\"time\":"<<n.timeMs<<",\"lane\":"<<n.lane<<",\"length\":"<<n.sustainMs<<",\"noteType\":"<<quote(n.noteType)
            <<",\"gfNote\":"<<(n.gfNote?"true":"false")<<",\"altAnim\":"<<(n.altAnim?"true":"false")
            <<",\"side\":"<<quote(n.side==Side::Opponent?"opponent":"player")<<'}';}
    json<<"],\"bpm\":[";comma=false;for(const auto& b:chart_.bpmTimeline){if(comma)json<<',';comma=true;json<<"{\"time\":"<<b.timeMs<<",\"bpm\":"<<b.bpm<<'}';}
    const auto total=std::count_if(rhythm_.NoteStates().begin(),rhythm_.NoteStates().end(),[](const auto& n){return n.judgement.has_value();});
    const auto& s=rhythm_.Stats();json<<"],\"totalPlayed\":"<<total<<",\"stats\":{\"score\":"<<s.rawScore<<",\"combo\":"<<s.combo<<",\"maxCombo\":"<<s.maxCombo
        <<",\"sick\":"<<s.sick<<",\"good\":"<<s.good<<",\"bad\":"<<s.bad<<",\"miss\":"<<s.miss<<",\"accuracy\":"<<s.accuracyPercent<<"},\"inputs\":[";
    comma=false;for(const auto& e:replayInputs_){if(comma)json<<',';comma=true;json<<"{\"time\":"<<e.time<<",\"lane\":"<<e.lane<<",\"type\":"<<quote(e.up?"up":"down")<<'}';}
    json<<"]}";const auto result=runtime.CallJson("pkeBuildPracticeReplay",json.str());
    if(!result.ok)throw std::runtime_error(result.error);return result.json;
}
}
