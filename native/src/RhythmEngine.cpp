#include "pke/RhythmEngine.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace pke {
void RhythmEngine::Reset(const Chart& chart){
    notes_.clear(); notes_.reserve(chart.notes.size());
    for(const auto& n:chart.notes) {
        if(n.side==Side::Player) notes_.push_back(RuntimeNoteState{n,NoteStatus::Pending,std::nullopt,0.0});
    }
    stats_=PlayStats{}; keyDown_.fill(false);
}
void RhythmEngine::recalcAccuracy(){
    const int judged=stats_.sick+stats_.good+stats_.bad+stats_.miss;
    stats_.accuracyPercent = judged==0 ? 100.0 : (double(stats_.rawScore)/(double(judged)*350.0))*100.0;
}
void RhythmEngine::applyJudgement(RuntimeNoteState& st,Judgement j,double songTimeMs){
    st.judgement=j; st.hitTimeMs=songTimeMs;
    switch(j){
        case Judgement::Sick: stats_.rawScore+=350; stats_.health=std::min(2.0,stats_.health+0.040); ++stats_.sick; ++stats_.combo; break;
        case Judgement::Good: stats_.rawScore+=200; stats_.health=std::min(2.0,stats_.health+0.025); ++stats_.good; ++stats_.combo; break;
        case Judgement::Bad: stats_.rawScore+=100; stats_.health=std::min(2.0,stats_.health+0.010); ++stats_.bad; ++stats_.combo; break;
        case Judgement::Miss: ++stats_.miss; stats_.combo=0; stats_.health=std::max(0.0,stats_.health-0.080); break;
    }
    stats_.maxCombo=std::max(stats_.maxCombo,stats_.combo); recalcAccuracy();
}
std::optional<Judgement> RhythmEngine::KeyDown(int lane,double t){
    if(lane<0||lane>3) return std::nullopt;
    keyDown_[lane]=true;
    RuntimeNoteState* best=nullptr; double bestAbs=std::numeric_limits<double>::infinity();
    for(auto& st:notes_){
        if(st.status!=NoteStatus::Pending || st.note.lane!=lane) continue;
        const double d=std::fabs(t-st.note.timeMs); if(d<=cfg_.badMs && d<bestAbs){ best=&st; bestAbs=d; }
    }
    if(!best) return std::nullopt;
    Judgement j=bestAbs<=cfg_.sickMs?Judgement::Sick:(bestAbs<=cfg_.goodMs?Judgement::Good:Judgement::Bad);
    applyJudgement(*best,j,t);
    if(best->note.sustainMs>0){ best->status = (t-best->note.timeMs)<=cfg_.holdStartGraceMs ? NoteStatus::Holding : NoteStatus::SustainBroken; }
    else best->status=NoteStatus::Hit;
    return j;
}
void RhythmEngine::KeyUp(int lane,double t){
    if(lane<0||lane>3) return;
    keyDown_[lane]=false;
    for(auto& st:notes_){
        if(st.status!=NoteStatus::Holding || st.note.lane!=lane) continue;
        const double end=st.note.timeMs+st.note.sustainMs;
        st.status = t >= end-cfg_.holdReleaseGraceMs ? NoteStatus::SustainCompleted : NoteStatus::SustainBroken;
    }
}
void RhythmEngine::Update(double t){
    for(auto& st:notes_){
        if(st.status==NoteStatus::Pending && t>=st.note.timeMs+cfg_.missMs){ st.status=NoteStatus::Missed; applyJudgement(st,Judgement::Miss,t); }
        else if(st.status==NoteStatus::Holding){
            const double end=st.note.timeMs+st.note.sustainMs;
            if(t>=end) st.status=NoteStatus::SustainCompleted;
        }
    }
}
}
