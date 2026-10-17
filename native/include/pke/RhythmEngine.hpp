#pragma once
#include "pke/Chart.hpp"
#include <array>
#include <optional>
#include <vector>

namespace pke {
enum class Judgement { Sick, Good, Bad, Miss };
enum class NoteStatus { Pending, Hit, Missed, Holding, SustainCompleted, SustainBroken };
struct JudgementConfig {
    double sickMs{45.0}; double goodMs{90.0}; double badMs{135.0}; double missMs{10*(1000.0/60.0)};
    double holdStartGraceMs{60.0}; double holdReleaseGraceMs{90.0};
};
struct PlayStats {
    int rawScore{0}; int combo{0}; int maxCombo{0}; int sick{0}; int good{0}; int bad{0}; int miss{0};
    double health{1.0}; double accuracyPercent{};
};
struct RuntimeNoteState {
    Note note;
    NoteStatus status{NoteStatus::Pending};
    std::optional<Judgement> judgement;
    double hitTimeMs{0.0};
    std::vector<double> sustainTimes;
    std::size_t sustainCursor{};
    std::optional<double> releasedAt;
};
class RhythmEngine {
public:
    explicit RhythmEngine(JudgementConfig cfg = {}):cfg_(cfg){}
    void Reset(const Chart& chart);
    std::optional<Judgement> KeyDown(int lane,double songTimeMs);
    std::optional<Judgement> KeyDown(int lane,double judgementTimeMs,double rawSongTimeMs);
    void KeyUp(int lane,double songTimeMs);
    void Update(double songTimeMs);
    void Update(double rawSongTimeMs,double judgementTimeMs);
    const PlayStats& Stats() const { return stats_; }
    const std::vector<RuntimeNoteState>& NoteStates() const { return notes_; }
private:
    void applyJudgement(RuntimeNoteState&, Judgement, double);
    void recalcAccuracy();
    void breakHold(RuntimeNoteState&);
    JudgementConfig cfg_;
    std::vector<RuntimeNoteState> notes_;
    PlayStats stats_{};
    std::array<bool,4> keyDown_{};
    int judgedHeads_{};
    double weightedHits_{};
};
}
