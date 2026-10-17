#pragma once
#if __has_include(<SDL.h>)
#  include <SDL.h>
#elif __has_include(<SDL2/SDL.h>)
#  include <SDL2/SDL.h>
#else
using SDL_Scancode = int;
constexpr SDL_Scancode SDL_SCANCODE_UNKNOWN = 0;
constexpr SDL_Scancode SDL_SCANCODE_RIGHT = 79;
constexpr SDL_Scancode SDL_SCANCODE_LEFT  = 80;
constexpr SDL_Scancode SDL_SCANCODE_DOWN  = 81;
constexpr SDL_Scancode SDL_SCANCODE_UP    = 82;
struct SDL_Point { int x; int y; };
struct SDL_Rect { int x; int y; int w; int h; };
struct SDL_Renderer;
struct SDL_Texture;
#endif
