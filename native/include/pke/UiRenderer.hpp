#pragma once
#include "pke/CharacterAnimator.hpp"
#include "pke/Renderer.hpp"
#include <cstdint>

namespace pke {
struct UiColor {std::uint8_t r{},g{},b{},a{255};bool operator==(const UiColor&) const=default;};
bool Contains(SDL_Rect,double x,double y) noexcept;
class UiRenderer {
public:
    UiRenderer();~UiRenderer();
    UiRenderer(const UiRenderer&)=delete;UiRenderer& operator=(const UiRenderer&)=delete;
    bool Initialize(SDL_Renderer*,const std::filesystem::path& font={});
    void Begin(UiColor);
    void BeginOverlay();
    void End();
    void Panel(std::string_view slot,SDL_Rect,UiColor top,UiColor bottom,int radius=0);
    void Card(std::string_view slot,SDL_Rect,UiColor top,UiColor bottom,int radius=14,int border=3);
    bool Text(std::string_view slot,std::string_view text,SDL_Rect,int pointSize,UiColor,bool center=false);
    bool PreloadImage(std::string key,const std::filesystem::path&);
    bool Image(std::string_view key,SDL_Rect,bool leftHalf=false);
    bool Character(std::string_view key,const CharacterDefinition&,AnimationFrameView,SDL_Rect);
    Renderer::CacheCounters CacheStats() const noexcept;
    std::string LastError() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
