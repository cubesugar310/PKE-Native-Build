#pragma once
#include <string>
#include <vector>

namespace pke {
enum class Side { Player, Opponent };
struct Note {
    double timeMs{};
    int lane{};
    double sustainMs{};
    Side side{Side::Opponent};
    std::string noteType;
    bool altAnim{false};
    bool gfNote{false};
};
struct BpmPoint { double timeMs{}; double bpm{}; };
struct Chart {
    std::vector<Note> notes;
    std::vector<BpmPoint> bpmTimeline;
    double scrollSpeed{1.0};
    double baseBpm{100.0};
    std::string player1;
    std::string player2;
    std::string gfVersion;
    std::vector<std::string> warnings;
};
}
