#pragma once
#include "pke/Config.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace pke {
class App {
public:
    int Run(int argc,char** argv);
private:
    enum class State { SongSelection, Gameplay, FatalStartupError };
};
}
