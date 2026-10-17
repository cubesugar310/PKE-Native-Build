#include "pke/Renderer.hpp"
#include "pke/CharacterLoader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
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
double ComputeNoteY(double note,double song,double speed,double receptorY,double px){ return receptorY+(note-song)*speed*px; }
bool NeedsDiagnosticFallback(const CharacterRenderView& view,bool textureAvailable) noexcept { return view.definition!=nullptr && !textureAvailable; }

struct Renderer::Impl {
    SDL_Renderer* renderer{nullptr};
    std::string error;
#ifdef PKE_NATIVE_WITH_SDL
    std::unordered_map<std::string,SDL_Texture*> textures;
    TTF_Font* font{nullptr};
    struct TextSlot { std::string text; SDL_Texture* texture{nullptr}; int w{0}; int h{0}; } songText, statsText, judgementText, diagText;
    ~Impl(){ for(auto& [k,t]:textures) if(t) SDL_DestroyTexture(t); for(auto* slot:{&songText,&statsText,&judgementText,&diagText}) if(slot->texture) SDL_DestroyTexture(slot->texture); if(font) TTF_CloseFont(font); }
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
    auto* t=IMG_LoadTexture(impl_->renderer,d.imagePath.string().c_str()); if(!t){ impl_->error=IMG_GetError(); return false; } auto it=impl_->textures.find(key); if(it!=impl_->textures.end()&&it->second)SDL_DestroyTexture(it->second); impl_->textures[std::move(key)]=t; return true;
#else
    (void)key;(void)d; return true;
#endif
}
bool Renderer::PreloadFont(const std::filesystem::path& p,int size){
#ifdef PKE_NATIVE_WITH_SDL
    if(TTF_WasInit()==0 && TTF_Init()!=0){ impl_->error=TTF_GetError(); return false; } if(impl_->font)TTF_CloseFont(impl_->font); impl_->font=TTF_OpenFont(p.string().c_str(),size); if(!impl_->font){impl_->error=TTF_GetError();return false;} return true;
#else
    (void)p;(void)size;return true;
#endif
}
std::string Renderer::LastError() const { return impl_->error; }
#ifdef PKE_NATIVE_WITH_SDL
static void updateTextTexture(SDL_Renderer* renderer, TTF_Font* font, std::string_view text, std::string& cachedText, SDL_Texture*& texture, int& w, int& h){
    if(!font || text==cachedText) return;
    cachedText.assign(text); if(texture){ SDL_DestroyTexture(texture); texture=nullptr; }
    if(text.empty()){ w=h=0; return; }
    SDL_Color c{255,255,255,255}; SDL_Surface* surf=TTF_RenderUTF8_Blended(font,cachedText.c_str(),c); if(!surf) return; w=surf->w; h=surf->h; texture=SDL_CreateTextureFromSurface(renderer,surf); SDL_FreeSurface(surf);
}
#endif
void Renderer::Draw(const RenderState& state){
#ifdef PKE_NATIVE_WITH_SDL
    if(!impl_->renderer)return;
    auto drawCharacter=[&](const CharacterRenderView& cv){
        if(!cv.definition) return;
        auto it=impl_->textures.find(cv.textureKey);
        const bool textureAvailable=it!=impl_->textures.end()&&it->second;
        if(NeedsDiagnosticFallback(cv,textureAvailable)){
            SDL_Rect fallback{cv.basePosition.x+cv.definition->position.x,cv.basePosition.y+cv.definition->position.y,96,96};
            SDL_SetRenderDrawColor(impl_->renderer,220,40,180,255);
            SDL_RenderFillRect(impl_->renderer,&fallback);
            return;
        }
        if(cv.frame.atlasFrameIndex>=cv.definition->atlas.size()) return;
        const auto& af=cv.definition->atlas[cv.frame.atlasFrameIndex]; auto p=ComputeFramePlacement(af,{cv.basePosition.x+cv.definition->position.x,cv.basePosition.y+cv.definition->position.y},cv.definition->scale,cv.definition->flipX);
        SDL_Rect dst{int(std::lround(p.destinationX)),int(std::lround(p.destinationY)),int(std::lround(p.destinationW)),int(std::lround(p.destinationH))};
        SDL_RendererFlip flip=p.flipX?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE; SDL_RenderCopyEx(impl_->renderer,it->second,&p.source,&dst,p.rotated?-90.0:0.0,nullptr,flip);
    };
    auto drawSlot=[&](Impl::TextSlot& slot,std::string_view text,int x,int y){ updateTextTexture(impl_->renderer,impl_->font,text,slot.text,slot.texture,slot.w,slot.h); if(slot.texture){SDL_Rect d{x,y,slot.w,slot.h};SDL_RenderCopy(impl_->renderer,slot.texture,nullptr,&d);} };
    int ow=0,oh=0; SDL_GetRendererOutputSize(impl_->renderer,&ow,&oh); auto vp=ComputeLetterboxedViewport(ow,oh); SDL_RenderSetViewport(impl_->renderer,&vp.destination); SDL_RenderSetScale(impl_->renderer,float(vp.scale),float(vp.scale));
    SDL_SetRenderDrawColor(impl_->renderer,20,20,28,255); SDL_RenderClear(impl_->renderer);
    drawCharacter(state.dad); drawCharacter(state.bf);
    const int receptorY=180; const int playerX[4]={1120,1240,1360,1480}; const int oppX[4]={320,440,560,680}; SDL_SetRenderDrawColor(impl_->renderer,100,100,110,255); for(int i=0;i<4;++i){SDL_Rect a{playerX[i],receptorY,64,20},b{oppX[i],receptorY,64,20};SDL_RenderFillRect(impl_->renderer,&a);SDL_RenderFillRect(impl_->renderer,&b);}
    ForEachRenderableNote(state,[&](const Note& note){ double y=ComputeNoteY(note.timeMs,state.songTimeMs,state.scrollSpeed,receptorY); if(y<-200||y>1280)return; int x=(note.side==Side::Player?playerX[note.lane]:oppX[note.lane]); SDL_Rect n{x,int(std::lround(y)),64,28}; SDL_SetRenderDrawColor(impl_->renderer,220,220,240,255); SDL_RenderFillRect(impl_->renderer,&n); if(note.sustainMs>0){ double ey=ComputeNoteY(note.timeMs+note.sustainMs,state.songTimeMs,state.scrollSpeed,receptorY); SDL_Rect body{x+26,int(std::lround(std::min(y,ey))),12,int(std::lround(std::abs(ey-y)))}; SDL_RenderFillRect(impl_->renderer,&body); } });
    SDL_SetRenderDrawColor(impl_->renderer,60,60,60,255); SDL_Rect hb{560,980,800,24}; SDL_RenderFillRect(impl_->renderer,&hb); SDL_SetRenderDrawColor(impl_->renderer,80,220,120,255); SDL_Rect hp{560,980,int(std::clamp(state.stats.health/2.0,0.0,1.0)*800),24}; SDL_RenderFillRect(impl_->renderer,&hp);
    char stats[160]{}; std::snprintf(stats,sizeof(stats),"Score %d  Acc %.2f%%  Combo %d",state.stats.rawScore,state.stats.accuracyPercent,state.stats.combo);
    drawSlot(impl_->songText,state.songName,30,30); drawSlot(impl_->statsText,stats,30,70); drawSlot(impl_->judgementText,state.judgementText,900,720);
    if(state.diagnosticsVisible){ char diag[256]{}; const auto& d=state.diagnostics; const double fps=d.latestFrame.intervalMs>0.0?1000.0/d.latestFrame.intervalMs:0.0; std::snprintf(diag,sizeof(diag),"FPS %.1f  update %.3fms  render %.3fms  audio %llu (+%u)  input %+.2fms",fps,d.latestFrame.updateMs,d.latestFrame.renderMs,static_cast<unsigned long long>(d.latestAudio.songFrame),d.latestAudio.callbackFrames,d.latestInputOffsetMs); drawSlot(impl_->diagText,diag,30,110); }
    SDL_RenderSetScale(impl_->renderer,1,1); SDL_RenderSetViewport(impl_->renderer,nullptr); SDL_RenderPresent(impl_->renderer);
#else
    (void)state;
#endif
}
}
