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
    for(auto& state:notes_){
        const double end=state.note.timeMs+state.note.sustainMs;double cursor=state.note.timeMs;
        for(int guard=0;cursor<end-.01&&guard<2048;++guard){
            double bpm=chart.baseBpm>0?chart.baseBpm:120,nextChange=std::numeric_limits<double>::infinity();
            for(const auto& point:chart.bpmTimeline){if(point.timeMs<=cursor)bpm=point.bpm;else{nextChange=point.timeMs;break;}}
            double next=std::min(end,cursor+60000/std::max(1.0,bpm)/4);
            if(nextChange<next-.01)next=nextChange;if(!(next>cursor+.001))next=std::min(end,cursor+1);
            state.sustainTimes.push_back(cursor);cursor=next;
        }
    }
    stats_=PlayStats{}; keyDown_.fill(false);judgedHeads_=0;weightedHits_=0;
}
void RhythmEngine::recalcAccuracy(){
    stats_.accuracyPercent = judgedHeads_==0?0:weightedHits_/judgedHeads_*100;
}
void RhythmEngine::applyJudgement(RuntimeNoteState& st,Judgement j,double songTimeMs){
    st.judgement=j; st.hitTimeMs=songTimeMs;
    ++judgedHeads_;
    switch(j){
        case Judgement::Sick: weightedHits_+=1;stats_.rawScore+=350; stats_.health=std::min(2.0,stats_.health+0.040); ++stats_.sick; ++stats_.combo; break;
        case Judgement::Good: weightedHits_+=.75;stats_.rawScore+=200; stats_.health=std::min(2.0,stats_.health+0.025); ++stats_.good; ++stats_.combo; break;
        case Judgement::Bad: weightedHits_+=.4;stats_.rawScore+=100; stats_.health=std::min(2.0,stats_.health+0.010); ++stats_.bad; ++stats_.combo; break;
        case Judgement::Miss: stats_.rawScore-=10;++stats_.miss; stats_.combo=0; stats_.health=std::max(0.0,stats_.health-0.120); break;
    }
    stats_.maxCombo=std::max(stats_.maxCombo,stats_.combo); recalcAccuracy();
}
std::optional<Judgement> RhythmEngine::KeyDown(int lane,double t){
    return KeyDown(lane,t,t);
}
std::optional<Judgement> RhythmEngine::KeyDown(int lane,double t,double rawTime){
    if(lane<0||lane>3) return std::nullopt;
    keyDown_[lane]=true;
    for(auto& note:notes_)if(note.note.lane==lane&&note.status==NoteStatus::Holding){
        if(note.releasedAt&&std::min(rawTime,note.note.timeMs+note.note.sustainMs)-*note.releasedAt>cfg_.holdReleaseGraceMs)breakHold(note);
        else note.releasedAt.reset();
    }
    RuntimeNoteState* best=nullptr; double bestAbs=std::numeric_limits<double>::infinity();
    for(auto& st:notes_){
        if(st.status!=NoteStatus::Pending || st.note.lane!=lane) continue;
        const double d=std::fabs(t-st.note.timeMs); if(d<=cfg_.missMs && d<bestAbs){ best=&st; bestAbs=d; }
    }
    if(!best) return std::nullopt;
    Judgement j=bestAbs<=cfg_.sickMs?Judgement::Sick:(bestAbs<=cfg_.goodMs?Judgement::Good:Judgement::Bad);
    const auto time=best->note.timeMs;
    for(auto& note:notes_)if(note.status==NoteStatus::Pending&&note.note.lane==lane&&std::fabs(note.note.timeMs-time)<=.01){
        applyJudgement(note,j,t);note.status=note.note.sustainMs>0?NoteStatus::Holding:NoteStatus::Hit;
    }
    return j;
}
void RhythmEngine::KeyUp(int lane,double t){
    if(lane<0||lane>3) return;
    keyDown_[lane]=false;
    for(auto& st:notes_){
        if(st.status!=NoteStatus::Holding || st.note.lane!=lane) continue;
        if(!st.releasedAt)st.releasedAt=std::max(t,st.note.timeMs+cfg_.holdStartGraceMs);
    }
}
void RhythmEngine::Update(double t){
    Update(t,t);
}
void RhythmEngine::Update(double t,double judgementTime){
    for(auto& st:notes_){
        if(st.status==NoteStatus::Pending && judgementTime>st.note.timeMs+cfg_.missMs){ st.status=NoteStatus::Missed; applyJudgement(st,Judgement::Miss,judgementTime); }
        else if(st.status==NoteStatus::Holding){
            const double end=st.note.timeMs+st.note.sustainMs;
            if(st.releasedAt&&std::min(t,end)-*st.releasedAt>cfg_.holdReleaseGraceMs){breakHold(st);continue;}
            while(st.sustainCursor<st.sustainTimes.size()&&st.sustainTimes[st.sustainCursor]<=t+cfg_.sickMs){
                ++st.sustainCursor;stats_.health=std::min(2.0,stats_.health+.006);
            }
            if(t>=end)st.status=NoteStatus::SustainCompleted;
        }
    }
}
void RhythmEngine::breakHold(RuntimeNoteState& st){
    st.status=NoteStatus::SustainBroken;++stats_.miss;stats_.combo=0;stats_.rawScore-=10;stats_.health=std::max(0.0,stats_.health-.10);
}
}
