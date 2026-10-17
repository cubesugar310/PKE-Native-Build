#pragma once
#include "pke/Character.hpp"
#include "pke/CharacterAnimator.hpp"
#include "pke/Diagnostics.hpp"
#include "pke/RhythmEngine.hpp"
#include "pke/SdlCompat.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pke {
struct CharacterTransform;
inline constexpr int kLogicalWidth=1280,kLogicalHeight=720;
struct Viewport { SDL_Rect destination{}; double scale{}; };
Viewport ComputeLetterboxedViewport(int outputW,int outputH,int logicalW=kLogicalWidth,int logicalH=kLogicalHeight);
struct LogicalPoint {double x{},y{};};
// SDL mouse coordinates are window units; renderer output may have a different
// pixel density. Reject points in bars and outside the logical canvas.
std::optional<LogicalPoint> WindowToLogical(double x,double y,int windowW,int windowH,int outputW,int outputH);
double ComputeNoteY(double noteTimeMs,double songTimeMs,double scrollSpeed,double receptorY,double pixelsPerMsAtSpeed1=0.45);

struct CharacterRenderView {
    std::string textureKey;
    const CharacterDefinition* definition{nullptr};
    AnimationFrameView frame{};
    SDL_Point basePosition{};
    double presentationScale{1.0};
    const CharacterTransform* originalTransform{};
    bool sourcePlayer{true},opponentPerspective{};
};
bool NeedsDiagnosticFallback(const CharacterRenderView& view,bool textureAvailable) noexcept;
struct RenderState {
    double songTimeMs{0.0};
    double scrollSpeed{1.0};
    bool downScroll{},cornerJudgement{};
    const Chart* chart{nullptr};
    const std::vector<RuntimeNoteState>* notes{nullptr};
    CharacterRenderView bf;
    CharacterRenderView dad;
    PlayStats stats{};
    std::string_view judgementText{};
    std::string_view songName{};
    bool diagnosticsVisible{false};
    DiagnosticsSummary diagnostics{};
};

template<class Fn>
void ForEachRenderableNote(const RenderState& state,Fn&& fn){
    if(state.chart){
        for(const auto& note:state.chart->notes) if(note.side==Side::Opponent) fn(note);
    }
    if(state.notes){
        for(const auto& runtime:*state.notes){
            if(runtime.note.side!=Side::Player) continue;
            if(runtime.status==NoteStatus::Missed||runtime.status==NoteStatus::Hit||runtime.status==NoteStatus::SustainCompleted) continue;
            fn(runtime.note);
        }
    }
}

class Renderer {
public:
    struct CacheCounters {std::uint64_t imageLoads{},textRasterizations{},textCacheHits{};};
    Renderer();
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    bool Initialize(SDL_Renderer* renderer);
    bool PreloadCharacter(std::string key,const CharacterDefinition& definition);
    bool PreloadFont(const std::filesystem::path& fontPath,int pointSize=28);
    void Draw(const RenderState& state,bool present=true);
    std::string LastError() const;
    CacheCounters CacheStats() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
