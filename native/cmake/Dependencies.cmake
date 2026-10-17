include(FetchContent)
# Exact dependency versions for the Windows/full build. The offline-core test
# mode intentionally skips FetchContent so CI/sandboxes without network can
# verify deterministic core logic.
set(PKE_SDL_TAG release-2.30.12)
set(PKE_SDL_IMAGE_TAG release-2.8.4)
set(PKE_SDL_TTF_TAG release-2.22.0)
set(PKE_MINIAUDIO_TAG 0.11.21)
set(PKE_STB_COMMIT f0569113c93ad095470c54bf34a17b36646bbbb5)
set(PKE_JSON_TAG v3.11.3)
set(PKE_TINYXML2_TAG 10.0.0)
set(PKE_CATCH2_TAG v3.7.1)

FetchContent_Declare(SDL2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG ${PKE_SDL_TAG} GIT_SHALLOW TRUE)
FetchContent_Declare(SDL2_image GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git GIT_TAG ${PKE_SDL_IMAGE_TAG} GIT_SHALLOW TRUE)
FetchContent_Declare(SDL2_ttf GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git GIT_TAG ${PKE_SDL_TTF_TAG} GIT_SHALLOW TRUE)
FetchContent_Declare(miniaudio GIT_REPOSITORY https://github.com/mackron/miniaudio.git GIT_TAG ${PKE_MINIAUDIO_TAG} GIT_SHALLOW TRUE)
FetchContent_Declare(stb GIT_REPOSITORY https://github.com/nothings/stb.git GIT_TAG ${PKE_STB_COMMIT})
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG ${PKE_JSON_TAG} GIT_SHALLOW TRUE)
FetchContent_Declare(tinyxml2 GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git GIT_TAG ${PKE_TINYXML2_TAG} GIT_SHALLOW TRUE)
FetchContent_Declare(Catch2 GIT_REPOSITORY https://github.com/catchorg/Catch2.git GIT_TAG ${PKE_CATCH2_TAG} GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL2 SDL2_image SDL2_ttf miniaudio stb nlohmann_json tinyxml2 Catch2)
