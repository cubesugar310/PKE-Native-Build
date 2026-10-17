#include "pke/UiRenderer.hpp"
#include "pke/CharacterLoader.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <unordered_map>
#ifdef PKE_NATIVE_WITH_SDL
#include <SDL_image.h>
#include <SDL_ttf.h>
#endif
namespace pke {
bool Contains(SDL_Rect r,double x,double y) noexcept{return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
struct UiRenderer::Impl {
    SDL_Renderer* renderer{};std::string error;Renderer::CacheCounters counters;
#ifdef PKE_NATIVE_WITH_SDL
    struct DeleteTexture{void operator()(SDL_Texture* p) const noexcept{if(p)SDL_DestroyTexture(p);}};
    struct DeleteFont{void operator()(TTF_Font* p) const noexcept{if(p)TTF_CloseFont(p);}};
    using Texture=std::unique_ptr<SDL_Texture,DeleteTexture>;
    struct PanelSlot{int w{},h{},radius{};UiColor top,bottom;Texture texture;};
    struct TextSlot{std::string text;int size{},width{},w{},h{};UiColor color;Texture texture;};
    struct ImageSlot{std::filesystem::path path;Texture texture;int w{},h{};};
    std::unordered_map<std::string,PanelSlot> panels;
    std::unordered_map<std::string,TextSlot> texts;
    std::unordered_map<std::string,ImageSlot> images;
    std::vector<char> uiFont,fallbackFont;
    std::map<std::pair<int,bool>,std::unique_ptr<TTF_Font,DeleteFont>> fonts;
    TTF_Font* Font(int size,bool fallback=false){
        if(fallback&&fallbackFont.empty())fallback=false;
        const auto key=std::pair{size,fallback};if(auto it=fonts.find(key);it!=fonts.end())return it->second.get();
        auto& bytes=fallback?fallbackFont:uiFont;if(bytes.empty())return nullptr;
        auto* stream=SDL_RWFromConstMem(bytes.data(),static_cast<int>(bytes.size()));if(!stream)return nullptr;
        std::unique_ptr<TTF_Font,DeleteFont> font(TTF_OpenFontRW(stream,1,size));if(!font){error=TTF_GetError();return nullptr;}
        TTF_SetFontStyle(font.get(),TTF_STYLE_BOLD);auto* result=font.get();fonts.emplace(key,std::move(font));return result;
    }
#endif
};
UiRenderer::UiRenderer():impl_(std::make_unique<Impl>()){}
UiRenderer::~UiRenderer()=default;
bool UiRenderer::Initialize(SDL_Renderer* renderer,const std::filesystem::path& requested){
    impl_->renderer=renderer;
#ifdef PKE_NATIVE_WITH_SDL
    if(!renderer)return false;if(TTF_WasInit()==0&&TTF_Init()!=0){impl_->error=TTF_GetError();return false;}
    const auto read=[](const std::filesystem::path& path,std::vector<char>& out){
        std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)return false;const auto size=input.tellg();
        if(size<=0||size>96*1024*1024)return false;std::vector<char> value(static_cast<std::size_t>(size));input.seekg(0);
        if(!input.read(value.data(),size))return false;out=std::move(value);return true;};
    if(!requested.empty()){if(!read(requested,impl_->uiFont)){impl_->error="Cannot read UI font";return false;}}
    else {
        std::vector<std::filesystem::path> candidates;
        if(const auto* windir=std::getenv("WINDIR"))for(const auto* name:{"segoeui.ttf","arial.ttf","meiryo.ttc"})candidates.push_back(std::filesystem::path(windir)/"Fonts"/name);
        for(const auto* path:{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/opentype/urw-base35/NimbusSans-Regular.otf"})candidates.emplace_back(path);
        for(const auto& path:candidates)if(read(path,impl_->uiFont))break;
    }
    std::vector<std::filesystem::path> fallback;
    if(const auto* windir=std::getenv("WINDIR"))for(const auto* name:{"meiryo.ttc","YuGothM.ttc","msgothic.ttc","msyh.ttc"})fallback.push_back(std::filesystem::path(windir)/"Fonts"/name);
    if(const auto* path=std::getenv("PKE_UI_CJK_FONT"))fallback.emplace_back(path);
    fallback.emplace_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
    for(const auto& path:fallback)if(read(path,impl_->fallbackFont))break;
    if(!impl_->Font(20)){impl_->error="No usable system UI font";return false;}
#endif
    return true;
}
void UiRenderer::BeginOverlay(){
#ifdef PKE_NATIVE_WITH_SDL
    int w,h;SDL_GetRendererOutputSize(impl_->renderer,&w,&h);const auto viewport=ComputeLetterboxedViewport(w,h);
    SDL_RenderSetScale(impl_->renderer,1,1);SDL_RenderSetViewport(impl_->renderer,&viewport.destination);
    SDL_RenderSetScale(impl_->renderer,float(viewport.scale),float(viewport.scale));
#endif
}
void UiRenderer::Begin(UiColor color){
#ifdef PKE_NATIVE_WITH_SDL
    SDL_RenderSetScale(impl_->renderer,1,1);SDL_RenderSetViewport(impl_->renderer,nullptr);
    SDL_SetRenderDrawColor(impl_->renderer,0,0,0,255);SDL_RenderClear(impl_->renderer);BeginOverlay();
    SDL_SetRenderDrawColor(impl_->renderer,color.r,color.g,color.b,color.a);SDL_Rect canvas{0,0,kLogicalWidth,kLogicalHeight};SDL_RenderFillRect(impl_->renderer,&canvas);
#else
    (void)color;
#endif
}
void UiRenderer::End(){
#ifdef PKE_NATIVE_WITH_SDL
    SDL_RenderSetScale(impl_->renderer,1,1);SDL_RenderSetViewport(impl_->renderer,nullptr);SDL_RenderPresent(impl_->renderer);
#endif
}
void UiRenderer::Panel(std::string_view slot,SDL_Rect rect,UiColor top,UiColor bottom,int radius){
#ifdef PKE_NATIVE_WITH_SDL
    if(rect.w<=0||rect.h<=0)return;const std::string key(slot);auto it=impl_->panels.find(key);
    if(it==impl_->panels.end()||it->second.w!=rect.w||it->second.h!=rect.h||it->second.top!=top||it->second.bottom!=bottom||it->second.radius!=radius){
        Impl::PanelSlot candidate;candidate.w=rect.w;candidate.h=rect.h;candidate.radius=radius;candidate.top=top;candidate.bottom=bottom;
        auto* surface=SDL_CreateRGBSurfaceWithFormat(0,rect.w,rect.h,32,SDL_PIXELFORMAT_RGBA32);if(!surface)return;
        SDL_FillRect(surface,nullptr,0);const auto r=std::min({radius,rect.w/2,rect.h/2});
        for(int y=0;y<rect.h;++y){const double t=rect.h>1?double(y)/(rect.h-1):0;
            const auto mix=[&](int a,int b){return static_cast<Uint8>(std::lround(a+(b-a)*t));};
            const auto color=SDL_MapRGBA(surface->format,mix(top.r,bottom.r),mix(top.g,bottom.g),mix(top.b,bottom.b),mix(top.a,bottom.a));
            const double dy=y<r?r-y-0.5:y>=rect.h-r?y-(rect.h-r)+0.5:0;
            const int inset=dy?static_cast<int>(std::ceil(r-std::sqrt(std::max(0.0,double(r*r)-dy*dy)))):0;
            auto* row=reinterpret_cast<Uint32*>(static_cast<unsigned char*>(surface->pixels)+y*surface->pitch);
            std::fill(row+inset,row+rect.w-inset,color);
        }
        candidate.texture.reset(SDL_CreateTextureFromSurface(impl_->renderer,surface));SDL_FreeSurface(surface);if(!candidate.texture)return;
        SDL_SetTextureBlendMode(candidate.texture.get(),SDL_BLENDMODE_BLEND);
        auto [entry,inserted]=impl_->panels.try_emplace(key,std::move(candidate));if(!inserted)entry->second=std::move(candidate);it=entry;
    }
    SDL_RenderCopy(impl_->renderer,it->second.texture.get(),nullptr,&rect);
#else
    (void)slot;(void)rect;(void)top;(void)bottom;(void)radius;
#endif
}
void UiRenderer::Card(std::string_view slot,SDL_Rect rect,UiColor top,UiColor bottom,int radius,int border){
    const std::string key(slot);auto shadow=rect;shadow.y+=8;Panel(key+"-shadow",shadow,{0,0,0,80},{0,0,0,80},radius);
    Panel(key+"-border",rect,{17,17,17,255},{17,17,17,255},radius);
    rect.x+=border;rect.y+=border;rect.w-=2*border;rect.h-=2*border;Panel(key,rect,top,bottom,std::max(0,radius-border));
}
bool UiRenderer::Text(std::string_view slot,std::string_view text,SDL_Rect rect,int size,UiColor color,bool center){
#ifdef PKE_NATIVE_WITH_SDL
    if(text.empty())return true;if(rect.w<=0||rect.h<=0)return false;
    const std::string key(slot);auto it=impl_->texts.find(key);
    if(it==impl_->texts.end()||it->second.text!=text||it->second.size!=size||it->second.color!=color||it->second.width!=rect.w){
        auto* font=impl_->Font(size);if(!font)return false;
        // The fallback is loaded into memory during initialization as well.
        if(std::any_of(text.begin(),text.end(),[](unsigned char c){return c>=0x80;})&&!impl_->fallbackFont.empty())font=impl_->Font(size,true);
        Impl::TextSlot candidate;candidate.text=text;candidate.size=size;candidate.color=color;candidate.width=rect.w;
        ++impl_->counters.textRasterizations;auto* surface=TTF_RenderUTF8_Blended_Wrapped(font,candidate.text.c_str(),{color.r,color.g,color.b,color.a},static_cast<Uint32>(rect.w));
        if(!surface){impl_->error=TTF_GetError();return false;}candidate.w=surface->w;candidate.h=surface->h;
        candidate.texture.reset(SDL_CreateTextureFromSurface(impl_->renderer,surface));SDL_FreeSurface(surface);if(!candidate.texture)return false;
        auto [entry,inserted]=impl_->texts.try_emplace(key,std::move(candidate));if(!inserted)entry->second=std::move(candidate);it=entry;
    }else ++impl_->counters.textCacheHits;
    const auto& cached=it->second;SDL_Rect destination{rect.x+(center?(rect.w-cached.w)/2:0),rect.y+(center?(rect.h-cached.h)/2:0),cached.w,cached.h};
    SDL_RenderSetClipRect(impl_->renderer,&rect);SDL_RenderCopy(impl_->renderer,cached.texture.get(),nullptr,&destination);SDL_RenderSetClipRect(impl_->renderer,nullptr);return true;
#else
    (void)slot;(void)text;(void)rect;(void)size;(void)color;(void)center;return true;
#endif
}
bool UiRenderer::PreloadImage(std::string key,const std::filesystem::path& path){
#ifdef PKE_NATIVE_WITH_SDL
    if(auto it=impl_->images.find(key);it!=impl_->images.end()&&it->second.path==path)return true;
    Impl::ImageSlot candidate;candidate.path=path;candidate.texture.reset(IMG_LoadTexture(impl_->renderer,path.string().c_str()));
    if(!candidate.texture){impl_->error=IMG_GetError();return false;}SDL_QueryTexture(candidate.texture.get(),nullptr,nullptr,&candidate.w,&candidate.h);
    auto [entry,inserted]=impl_->images.try_emplace(std::move(key),std::move(candidate));if(!inserted)entry->second=std::move(candidate);++impl_->counters.imageLoads;
#else
    (void)key;(void)path;
#endif
    return true;
}
bool UiRenderer::Image(std::string_view key,SDL_Rect rect,bool leftHalf){
#ifdef PKE_NATIVE_WITH_SDL
    const auto it=impl_->images.find(std::string(key));if(it==impl_->images.end())return false;
    SDL_Rect source{0,0,leftHalf?it->second.w/2:it->second.w,it->second.h};
    return SDL_RenderCopy(impl_->renderer,it->second.texture.get(),&source,&rect)==0;
#else
    (void)key;(void)rect;(void)leftHalf;return false;
#endif
}
bool UiRenderer::Character(std::string_view key,const CharacterDefinition& def,AnimationFrameView frame,SDL_Rect box){
#ifdef PKE_NATIVE_WITH_SDL
    const auto it=impl_->images.find(std::string(key));if(it==impl_->images.end()||frame.atlasFrameIndex>=def.atlas.size())return false;
    const auto& atlas=def.atlas[frame.atlasFrameIndex];const auto fullW=atlas.frameWidth>0?atlas.frameWidth:atlas.source.w;
    const auto fullH=atlas.frameHeight>0?atlas.frameHeight:atlas.source.h;if(fullW<=0||fullH<=0)return false;
    const auto scale=std::min(double(box.w)/fullW,double(box.h)/fullH)*0.9;
    SDL_Point offset{};if(const auto anim=def.animations.find(std::string(frame.animationName));anim!=def.animations.end())offset=anim->second.offset;
    const SDL_Point origin{int(std::lround(box.x+(box.w-fullW*scale)/2-offset.x*scale)),int(std::lround(box.y+(box.h-fullH*scale)/2-offset.y*scale))};
    const auto placement=ComputeFramePlacement(atlas,origin,scale,def.flipX);
    SDL_FRect destination{float(placement.destinationX),float(placement.destinationY),float(placement.destinationW),float(placement.destinationH)};
    SDL_SetTextureScaleMode(it->second.texture.get(),def.noAntialiasing?SDL_ScaleModeNearest:SDL_ScaleModeLinear);
    return SDL_RenderCopyExF(impl_->renderer,it->second.texture.get(),&placement.source,&destination,placement.rotated?-90:0,nullptr,placement.flipX?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE)==0;
#else
    (void)key;(void)def;(void)frame;(void)box;return false;
#endif
}
Renderer::CacheCounters UiRenderer::CacheStats() const noexcept{return impl_->counters;}
std::string UiRenderer::LastError() const{return impl_->error;}
}
