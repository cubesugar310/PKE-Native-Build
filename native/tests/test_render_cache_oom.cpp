#define SDL_MAIN_HANDLED
#include "pke/Renderer.hpp"
#include <SDL.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {std::atomic<int> failAfter{-1};}
void* operator new(std::size_t size){
    const auto remaining=failAfter.load();
    if(remaining>=0&&failAfter.fetch_sub(1)==0){failAfter.store(-1);throw std::bad_alloc();}
    if(auto* value=std::malloc(size?size:1))return value;throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* value) noexcept{std::free(value);}
void operator delete[](void* value) noexcept{std::free(value);}
void operator delete(void* value,std::size_t) noexcept{std::free(value);}
void operator delete[](void* value,std::size_t) noexcept{std::free(value);}

int main(){
    const auto root=std::filesystem::temp_directory_path()/("pke-cache-oom-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto oldPath=root/"old.bmp",newPath=root/(std::string(120,'b')+".bmp");
    auto* pixel=SDL_CreateRGBSurfaceWithFormat(0,4,4,32,SDL_PIXELFORMAT_RGBA32);
    if(!pixel)return 2;SDL_FillRect(pixel,nullptr,SDL_MapRGBA(pixel->format,255,0,0,255));
    if(SDL_SaveBMP(pixel,oldPath.string().c_str())!=0)return 2;
    SDL_FillRect(pixel,nullptr,SDL_MapRGBA(pixel->format,0,0,255,255));
    if(SDL_SaveBMP(pixel,newPath.string().c_str())!=0)return 2;SDL_FreeSurface(pixel);
    int failures=0,caught=0;
    for(int allocation=0;allocation<16;++allocation){
        auto* surface=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_RGBA32);
        auto* sdl=SDL_CreateSoftwareRenderer(surface);if(!surface||!sdl)return 2;
        {
            pke::Renderer renderer;if(!renderer.Initialize(sdl))return 2;
            pke::CharacterDefinition oldDefinition;oldDefinition.imagePath=oldPath;
            oldDefinition.atlas.push_back({"idle",{0,0,4,4},0,0,4,4,false});
            auto nextDefinition=oldDefinition;nextDefinition.imagePath=newPath;
            if(!renderer.PreloadCharacter("bf",oldDefinition))return 2;
            bool threw=false;failAfter.store(allocation);
            try{renderer.PreloadCharacter("bf",nextDefinition);}catch(const std::bad_alloc&){threw=true;}
            failAfter.store(-1);
            if(threw){
                ++caught;
                // No read of oldPath is needed: its already loaded texture
                // must survive every failed replacement allocation.
                pke::RenderState state;state.bf={"bf",&oldDefinition,{0,"idle",0},{100,200}};renderer.Draw(state);
                const auto* row=static_cast<unsigned char*>(surface->pixels)+200*surface->pitch;
                Uint8 r,g,b,a;SDL_GetRGBA(reinterpret_cast<const Uint32*>(row)[100],surface->format,&r,&g,&b,&a);
                if(r!=255||g!=0||b!=0)++failures;
            }
        }
        SDL_DestroyRenderer(sdl);SDL_FreeSurface(surface);
    }
    std::filesystem::remove_all(root);
    if(!caught||failures){std::cerr<<"Texture replacement allocation regression: "<<failures<<" failures, "<<caught<<" injected exceptions\n";return 1;}
    std::cout<<"[PASS] Failed texture replacements retain the valid cached image ("<<caught<<" allocation failures)\n";
}
