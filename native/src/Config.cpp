#include "pke/Config.hpp"
#include <fstream>
#include <regex>
#include <optional>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef PKE_NATIVE_USE_NLOHMANN
#include <nlohmann/json.hpp>
#endif

namespace pke {
namespace {
std::filesystem::path ResolveRoot(const std::filesystem::path& executableDir,
                                  const std::filesystem::path& value) {
    if (value.is_absolute()) return value.lexically_normal();
    return (executableDir / value).lexically_normal();
}

#ifdef PKE_NATIVE_USE_NLOHMANN
SDL_Scancode KeyFromName(const std::string& value) {
    if (value == "LEFT" || value == "Left" || value == "left") return SDL_SCANCODE_LEFT;
    if (value == "DOWN" || value == "Down" || value == "down") return SDL_SCANCODE_DOWN;
    if (value == "UP" || value == "Up" || value == "up") return SDL_SCANCODE_UP;
    if (value == "RIGHT" || value == "Right" || value == "right") return SDL_SCANCODE_RIGHT;
    return SDL_SCANCODE_UNKNOWN;
}
#endif

#ifndef PKE_NATIVE_USE_NLOHMANN
std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open config: " + path.string());
    std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}
std::optional<std::string> StringField(const std::string& s, const char* key) {
    std::regex r(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch m; if (std::regex_search(s,m,r)) return m[1].str(); return std::nullopt;
}
std::optional<double> NumberField(const std::string& s, const char* key) {
    std::regex r(std::string("\\\"") + key + "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch m; if (std::regex_search(s,m,r)) return std::stod(m[1].str()); return std::nullopt;
}
std::optional<bool> BoolField(const std::string& s, const char* key) {
    std::regex r(std::string("\\\"") + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch m; if (std::regex_search(s,m,r)) return m[1].str()=="true"; return std::nullopt;
}
#endif
}

Config LoadConfig(const std::filesystem::path& executableDir,
                  const std::filesystem::path& configPath) {
    Config cfg;
    cfg.songsRoot = ResolveRoot(executableDir, "Songs");
    cfg.charactersRoot = ResolveRoot(executableDir, "Characters");
    if (!std::filesystem::exists(configPath)) return cfg;

#ifdef PKE_NATIVE_USE_NLOHMANN
    std::ifstream in(configPath);
    if (!in) throw std::runtime_error("Failed to open config: " + configPath.string());
    nlohmann::json j;
    try { in >> j; } catch (const std::exception& e) {
        throw std::runtime_error("Malformed config " + configPath.string() + ": " + e.what());
    }
    if (j.contains("songsRoot")) cfg.songsRoot = ResolveRoot(executableDir, j.at("songsRoot").get<std::string>());
    if (j.contains("charactersRoot")) cfg.charactersRoot = ResolveRoot(executableDir, j.at("charactersRoot").get<std::string>());
    if (j.contains("vsync")) cfg.vsync = j.at("vsync").get<bool>();
    if (j.contains("fpsCap")) cfg.fpsCap = j.at("fpsCap").get<int>();
    if (j.contains("inputOffsetMs")) cfg.inputOffsetMs = j.at("inputOffsetMs").get<double>();
    if (j.contains("audioLatencyCompensationMs")) cfg.audioLatencyCompensationMs = j.at("audioLatencyCompensationMs").get<double>();
    if (j.contains("playerKeys")) {
        auto keys = j.at("playerKeys").get<std::vector<std::string>>();
        if (keys.size() != 4) throw std::runtime_error("playerKeys must contain exactly 4 entries in " + configPath.string());
        for (size_t i=0;i<4;++i) {
            auto sc = KeyFromName(keys[i]);
            if (sc == SDL_SCANCODE_UNKNOWN) throw std::runtime_error("Unsupported key '" + keys[i] + "' in " + configPath.string());
            cfg.playerKeys[i] = sc;
        }
    }
#else
    const auto s = ReadFile(configPath);
    if (auto v=StringField(s,"songsRoot")) cfg.songsRoot=ResolveRoot(executableDir,*v);
    if (auto v=StringField(s,"charactersRoot")) cfg.charactersRoot=ResolveRoot(executableDir,*v);
    if (auto v=BoolField(s,"vsync")) cfg.vsync=*v;
    if (auto v=NumberField(s,"fpsCap")) cfg.fpsCap=static_cast<int>(*v);
    if (auto v=NumberField(s,"inputOffsetMs")) cfg.inputOffsetMs=*v;
    if (auto v=NumberField(s,"audioLatencyCompensationMs")) cfg.audioLatencyCompensationMs=*v;
#endif
    if (cfg.fpsCap < 30 || cfg.fpsCap > 1000) throw std::runtime_error("fpsCap out of range in " + configPath.string());
    return cfg;
}
}
