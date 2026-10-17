#include "pke/Renderer.hpp"
#include "pke/CharacterLoader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <utility>
#ifdef PKE_NATIVE_WITH_SDL
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#endif

namespace pke {
Viewport ComputeLetterboxedViewport(int ow,int oh,int lw,int lh){
    if(ow<=0||oh<=0||lw<=0||lh<=0) return {};
    const double scale=std::min(double(ow)/double(lw),double(oh)/double(lh));
    const int w=int(std::lround(lw*scale)),h=int(std::lround(lh*scale));
    return {{(ow-w)/2,(oh-h)/2,w,h},scale};
}
std::optional<LogicalPoint> WindowToLogical(double x,double y,int ww,int wh,int ow,int oh){
    if(ww<=0||wh<=0||ow<=0||oh<=0||!std::isfinite(x)||!std::isfinite(y))return {};
    const auto vp=ComputeLetterboxedViewport(ow,oh);
    const double pixelX=x*ow/ww,pixelY=y*oh/wh;
    if(pixelX<vp.destination.x||pixelY<vp.destination.y||
       pixelX>=vp.destination.x+vp.destination.w||pixelY>=vp.destination.y+vp.destination.h)return {};
    LogicalPoint point{(pixelX-vp.destination.x)/vp.scale,(pixelY-vp.destination.y)/vp.scale};
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||point.x<0||point.y<0||
       point.x>=kLogicalWidth||point.y>=kLogicalHeight)return {};
    return point;
}
double ComputeNoteY(double note,double song,double speed,double receptorY,double px){ return receptorY+(note-song)*speed*px; }
bool NeedsDiagnosticFallback(const CharacterRenderView& view,bool textureAvailable) noexcept { return view.definition!=nullptr && !textureAvailable; }

struct Renderer::Impl {
    SDL_Renderer* renderer{nullptr};
    std::string error;
    CacheCounters counters;
#ifdef PKE_NATIVE_WITH_SDL
    struct TextureDeleter {void operator()(SDL_Texture* value) const noexcept {if(value)SDL_DestroyTexture(value);}};
    using Texture=std::unique_ptr<SDL_Texture,TextureDeleter>;
    struct TextureSlot {std::filesystem::path path;Texture texture;};
    std::unordered_map<std::string,TextureSlot> textures;
    TTF_Font* font{nullptr};
    std::filesystem::path fontPath;
    int fontSize{};
    struct TextSlot { std::string text; SDL_Texture* texture{nullptr}; int w{0}; int h{0}; } songText, statsText, judgementText, diagText;
    ~Impl(){ for(auto* slot:{&songText,&statsText,&judgementText,&diagText}) if(slot->texture) SDL_DestroyTexture(slot->texture); if(font) TTF_CloseFont(font); }
#else
    ~Impl()=default;
#endif
};
Renderer::Renderer():impl_(std::make_unique<Impl>()){}
Renderer::~Renderer()=default;
bool Renderer::Initialize(SDL_Renderer* r){ impl_->renderer=r;
#ifdef PKE_NATIVE_WITH_SDL
    if(!r){ impl_->error="SDL renderer is null"; return false; }
#endif
    return true;
}
bool Renderer::PreloadCharacter(std::string key,const CharacterDefinition& d){
#ifdef PKE_NATIVE_WITH_SDL
    if(!impl_->renderer){ impl_->error="Renderer not initialized"; return false; }
    const auto known=impl_->textures.find(key);
    if(known!=impl_->textures.end()&&known->second.path==d.imagePath)return true;
    Impl::TextureSlot candidate{d.imagePath,{}};
    candidate.texture.reset(IMG_LoadTexture(impl_->renderer,d.imagePath.string().c_str()));
    if(!candidate.texture){impl_->error=IMG_GetError();return false;}
    // The candidate owns its texture through any path/key/map allocation.
    // Replacing an existing slot moves both fields without allocation.
    static_assert(noexcept(std::declval<Impl::TextureSlot&>()=std::declval<Impl::TextureSlot&&>()));
    auto [slot,inserted]=impl_->textures.try_emplace(std::move(key),std::move(candidate));
    if(!inserted)slot->second=std::move(candidate);
    ++impl_->counters.imageLoads;return true;
#else
    (void)key;(void)d; return true;
#endif
}
bool Renderer::PreloadFont(const std::filesystem::path& p,int size){
#ifdef PKE_NATIVE_WITH_SDL
    if(impl_->font&&impl_->fontPath==p&&impl_->fontSize==size)return true;
    if(TTF_WasInit()==0 && TTF_Init()!=0){ impl_->error=TTF_GetError(); return false; }
    auto candidatePath=p;
    auto* candidate=TTF_OpenFont(p.string().c_str(),size);if(!candidate){impl_->error=TTF_GetError();return false;}
    if(impl_->font)TTF_CloseFont(impl_->font);impl_->font=candidate;impl_->fontPath=std::move(candidatePath);impl_->fontSize=size;
    for(auto* slot:{&impl_->songText,&impl_->statsText,&impl_->judgementText,&impl_->diagText}){
        if(slot->texture)SDL_DestroyTexture(slot->texture);*slot={};
    }
    return true;
#else
    (void)p;(void)size;return true;
#endif
}
std::string Renderer::LastError() const { return impl_->error; }
Renderer::CacheCounters Renderer::CacheStats() const noexcept{return impl_->counters;}
#ifdef PKE_NATIVE_WITH_SDL
static void updateTextTexture(SDL_Renderer* renderer, TTF_Font* font, std::string_view text, std::string& cachedText, SDL_Texture*& texture, int& w, int& h,Renderer::CacheCounters& counters){
    if(!font)return;if(text==cachedText){++counters.textCacheHits;return;}
    if(text.empty()){if(texture)SDL_DestroyTexture(texture);texture=nullptr;cachedText.clear();w=h=0;return;}
    std::string value(text);SDL_Color c{255,255,255,255};++counters.textRasterizations;
    SDL_Surface* surface=TTF_RenderUTF8_Blended(font,value.c_str(),c);if(!surface)return;
    auto* candidate=SDL_CreateTextureFromSurface(renderer,surface);
    if(candidate){if(texture)SDL_DestroyTexture(texture);texture=candidate;cachedText=std::move(value);w=surface->w;h=surface->h;}
    SDL_FreeSurface(surface);
}
#endif
void Renderer::Draw(const RenderState& state,bool present){
#ifdef PKE_NATIVE_WITH_SDL
    if(!impl_->renderer)return;
    auto drawCharacter=[&](const CharacterRenderView& cv){
        if(!cv.definition) return;
        auto it=impl_->textures.find(cv.textureKey);
        const bool textureAvailable=it!=impl_->textures.end()&&it->second.texture;
        if(NeedsDiagnosticFallback(cv,textureAvailable)){
            SDL_Rect fallback{cv.basePosition.x+int(cv.definition->position.x*cv.presentationScale),cv.basePosition.y+int(cv.definition->position.y*cv.presentationScale),int(96*cv.presentationScale),int(96*cv.presentationScale)};
            SDL_SetRenderDrawColor(impl_->renderer,220,40,180,255);
            SDL_RenderFillRect(impl_->renderer,&fallback);
            return;
        }
        if(cv.frame.atlasFrameIndex>=cv.definition->atlas.size()) return;
        const auto& af=cv.definition->atlas[cv.frame.atlasFrameIndex]; auto p=ComputeFramePlacement(af,{cv.basePosition.x+int(std::lround(cv.definition->position.x*cv.presentationScale)),cv.basePosition.y+int(std::lround(cv.definition->position.y*cv.presentationScale))},cv.definition->scale*cv.presentationScale,cv.definition->flipX);
        SDL_Rect dst{int(std::lround(p.destinationX)),int(std::lround(p.destinationY)),int(std::lround(p.destinationW)),int(std::lround(p.destinationH))};
        SDL_RendererFlip flip=p.flipX?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE; SDL_RenderCopyEx(impl_->renderer,it->second.texture.get(),&p.source,&dst,p.rotated?-90.0:0.0,nullptr,flip);
    };
    auto drawSlot=[&](Impl::TextSlot& slot,std::string_view text,int x,int y){ updateTextTexture(impl_->renderer,impl_->font,text,slot.text,slot.texture,slot.w,slot.h,impl_->counters); if(slot.texture){SDL_Rect d{x,y,slot.w,slot.h};SDL_RenderCopy(impl_->renderer,slot.texture,nullptr,&d);} };
    int ow=0,oh=0; SDL_GetRendererOutputSize(impl_->renderer,&ow,&oh); auto vp=ComputeLetterboxedViewport(ow,oh);if(vp.scale<=0)return;
    SDL_RenderSetScale(impl_->renderer,1,1);SDL_RenderSetViewport(impl_->renderer,nullptr);
    SDL_SetRenderDrawColor(impl_->renderer,0,0,0,255);SDL_RenderClear(impl_->renderer);
    SDL_RenderSetViewport(impl_->renderer,&vp.destination);SDL_RenderSetScale(impl_->renderer,float(vp.scale),float(vp.scale));
    SDL_SetRenderDrawColor(impl_->renderer,20,20,28,255);SDL_Rect canvas{0,0,kLogicalWidth,kLogicalHeight};SDL_RenderFillRect(impl_->renderer,&canvas);
    drawCharacter(state.dad); drawCharacter(state.bf);
    // Original 1280x720 field centers and note-sprites vector geometry.
    constexpr int receptorY=94,playerX[4]={776,888,1000,1112},oppX[4]={98,210,322,434};
    constexpr SDL_Color laneColors[4]={{184,74,152,255},{80,240,240,255},{80,240,0,255},{240,48,64,255}};
    const auto arrow=[&](int lane,double x,double y,SDL_Color color){
        constexpr double points[7][2]={{-.18,.50},{.18,.50},{.18,-.08},{.48,-.08},{0,-.52},{-.48,-.08},{-.18,-.08}};
        constexpr double rotations[4]={-1.5707963267948966,3.141592653589793,0,1.5707963267948966};
        constexpr int triangles[9]={0,1,2,0,2,6,3,4,5};SDL_Vertex vertices[7]{};
        const double cosine=std::cos(rotations[lane]),sine=std::sin(rotations[lane]);
        for(int i=0;i<7;++i){vertices[i].position={float(x+98*(points[i][0]*cosine-points[i][1]*sine)),
            float(y+98*(points[i][0]*sine+points[i][1]*cosine))};vertices[i].color=color;}
        SDL_RenderGeometry(impl_->renderer,nullptr,vertices,7,triangles,9);
    };
    for(int lane=0;lane<4;++lane){arrow(lane,playerX[lane],receptorY,{100,100,110,255});arrow(lane,oppX[lane],receptorY,{100,100,110,255});}
    const auto drawNote=[&](const Note& note,NoteStatus status){
        if(note.lane<0||note.lane>3)return;
        const double y=ComputeNoteY(note.timeMs,state.songTimeMs,state.scrollSpeed,receptorY);
        const double endY=ComputeNoteY(note.timeMs+note.sustainMs,state.songTimeMs,state.scrollSpeed,receptorY);
        const int x=(note.side==Side::Player?playerX[note.lane]:oppX[note.lane]);const auto color=laneColors[note.lane];
        const bool holding=status==NoteStatus::Holding||status==NoteStatus::SustainBroken;
        if(note.sustainMs>0&&endY>=-100&&y<=820){
            const int start=std::max(-100,int(std::lround(holding?std::max(double(receptorY),y):y)));
            const int end=std::min(820,int(std::lround(endY)));
            SDL_SetRenderDrawColor(impl_->renderer,color.r,color.g,color.b,255);SDL_Rect body{x-9,start,18,std::max(0,end-start)};
            SDL_RenderFillRect(impl_->renderer,&body);
        }
        if(!holding&&y>=-100&&y<=820)arrow(note.lane,x,y,color);
    };
    if(state.chart)for(const auto& note:state.chart->notes)if(note.side==Side::Opponent)drawNote(note,NoteStatus::Pending);
    if(state.notes)for(const auto& note:*state.notes)if(note.status==NoteStatus::Pending||note.status==NoteStatus::Holding||note.status==NoteStatus::SustainBroken)drawNote(note.note,note.status);
    SDL_SetRenderDrawColor(impl_->renderer,60,60,60,255); SDL_Rect hb{373,653,534,16}; SDL_RenderFillRect(impl_->renderer,&hb); SDL_SetRenderDrawColor(impl_->renderer,80,220,120,255); SDL_Rect hp{373,653,int(std::clamp(state.stats.health/2.0,0.0,1.0)*534),16}; SDL_RenderFillRect(impl_->renderer,&hp);
    char stats[160]{}; std::snprintf(stats,sizeof(stats),"Score %d  Acc %.2f%%  Combo %d",state.stats.rawScore,state.stats.accuracyPercent,state.stats.combo);
    drawSlot(impl_->songText,state.songName,20,20); drawSlot(impl_->statsText,stats,20,47); drawSlot(impl_->judgementText,state.judgementText,600,480);
    if(state.diagnosticsVisible){ char diag[256]{}; const auto& d=state.diagnostics; const double fps=d.latestFrame.intervalMs>0.0?1000.0/d.latestFrame.intervalMs:0.0; std::snprintf(diag,sizeof(diag),"FPS %.1f  update %.3fms  render %.3fms  audio %llu (+%u)  input %+.2fms",fps,d.latestFrame.updateMs,d.latestFrame.renderMs,static_cast<unsigned long long>(d.latestAudio.songFrame),d.latestAudio.callbackFrames,d.latestInputOffsetMs); drawSlot(impl_->diagText,diag,20,73); }
    SDL_RenderSetScale(impl_->renderer,1,1); SDL_RenderSetViewport(impl_->renderer,nullptr); if(present)SDL_RenderPresent(impl_->renderer);
#else
    (void)state;(void)present;
#endif
}
}
