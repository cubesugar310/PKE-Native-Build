#include "pke/ChartParser.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <tuple>

#ifdef PKE_NATIVE_USE_NLOHMANN
#include <nlohmann/json.hpp>
#else
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#endif

namespace pke {
namespace {
struct OrderedNote { Note note; size_t order; };
Side mappedSide(bool mustHit, int noteData, NoteMappingMode mode) {
    if (mode == NoteMappingMode::AbsoluteNoteHalf) return noteData >= 4 ? Side::Opponent : Side::Player;
    bool player = mustHit;
    if (noteData > 3) player = !player;
    return player ? Side::Player : Side::Opponent;
}
std::string normalizeNoteTypeString(const std::string& s){ return (s=="null")?std::string{}:s; }
}

Chart ParseChart(const std::filesystem::path& jsonPath, ChartParserOptions options) {
    Chart out;
    std::vector<OrderedNote> ordered;
    try {
#ifdef PKE_NATIVE_USE_NLOHMANN
        std::ifstream in(jsonPath); if(!in) throw std::runtime_error("cannot open");
        nlohmann::json root; in >> root;
        const auto& song = root.contains("song") && root["song"].is_object() ? root["song"] : root;
        out.baseBpm = song.value("bpm",100.0);
        out.scrollSpeed = song.contains("scrollSpeed") ? song.value("scrollSpeed",1.0) : (song.contains("speed") ? song.value("speed",1.0) : root.value("scrollSpeed",1.0));
        out.player1=song.value("player1",""); out.player2=song.value("player2",""); out.gfVersion=song.value("gfVersion","");
        out.bpmTimeline.push_back({0.0,out.baseBpm}); double currentBpm=out.baseBpm, sectionTime=0.0; size_t order=0;
        if(song.contains("events") && song["events"].is_array() && !song["events"].empty()) out.warnings.push_back("Chart contains events; unsupported Phase 1 events are ignored");
        if(song.contains("notes") && song["notes"].is_array()) for(const auto& sec:song["notes"]){
            if(sec.value("changeBPM",false)) { double b=sec.value("bpm",currentBpm); if(b>0 && std::fabs(b-currentBpm)>1e-9){ currentBpm=b; out.bpmTimeline.push_back({sectionTime,currentBpm}); } }
            bool must=sec.value("mustHitSection",false), alt=sec.value("altAnim",false), gf=sec.value("gfSection",false);
            if(sec.contains("sectionNotes") && sec["sectionNotes"].is_array()) for(const auto& a:sec["sectionNotes"]){
                if(!a.is_array() || a.size()<3){ out.warnings.push_back("Ignored malformed section note"); continue; }
                int nd=a[1].get<int>(); Note n; n.timeMs=a[0].get<double>(); n.lane=((nd%4)+4)%4; n.sustainMs=a[2].get<double>(); n.side=mappedSide(must,nd,options.mappingMode); n.altAnim=alt; n.gfNote=gf;
                if(a.size()>3 && !a[3].is_null()) n.noteType=a[3].is_string()?a[3].get<std::string>():a[3].dump();
                ordered.push_back({std::move(n),order++});
            }
            const int steps=sec.value("lengthInSteps",16); sectionTime += steps * (60000.0/currentBpm/4.0);
        }
#else
        using boost::property_tree::ptree; ptree root; boost::property_tree::read_json(jsonPath.string(),root);
        const ptree* songPtr=&root; if(auto s=root.get_child_optional("song")) songPtr=&*s; const ptree& song=*songPtr;
        out.baseBpm=song.get<double>("bpm",100.0);
        if(auto v=song.get_optional<double>("scrollSpeed")) out.scrollSpeed=*v; else if(auto v=song.get_optional<double>("speed")) out.scrollSpeed=*v; else out.scrollSpeed=root.get<double>("scrollSpeed",1.0);
        out.player1=song.get<std::string>("player1",""); out.player2=song.get<std::string>("player2",""); out.gfVersion=song.get<std::string>("gfVersion","");
        out.bpmTimeline.push_back({0.0,out.baseBpm}); double currentBpm=out.baseBpm,sectionTime=0.0; size_t order=0;
        if(auto events=song.get_child_optional("events")){ if(!events->empty()) out.warnings.push_back("Chart contains events; unsupported Phase 1 events are ignored"); }
        if(auto sections=song.get_child_optional("notes")) for(const auto& secPair:*sections){ const auto& sec=secPair.second;
            if(sec.get<bool>("changeBPM",false)){ double b=sec.get<double>("bpm",currentBpm); if(b>0 && std::fabs(b-currentBpm)>1e-9){ currentBpm=b; out.bpmTimeline.push_back({sectionTime,currentBpm}); } }
            bool must=sec.get<bool>("mustHitSection",false),alt=sec.get<bool>("altAnim",false),gf=sec.get<bool>("gfSection",false);
            if(auto arr=sec.get_child_optional("sectionNotes")) for(const auto& notePair:*arr){ const auto& a=notePair.second; std::vector<std::string> vals; for(const auto& v:a) vals.push_back(v.second.get_value<std::string>());
                if(vals.size()<3){ out.warnings.push_back("Ignored malformed section note"); continue; }
                int nd=std::stoi(vals[1]); Note n; n.timeMs=std::stod(vals[0]); n.lane=((nd%4)+4)%4; n.sustainMs=std::stod(vals[2]); n.side=mappedSide(must,nd,options.mappingMode); n.altAnim=alt; n.gfNote=gf; if(vals.size()>3) n.noteType=normalizeNoteTypeString(vals[3]); ordered.push_back({std::move(n),order++}); }
            int steps=sec.get<int>("lengthInSteps",16); sectionTime += steps*(60000.0/currentBpm/4.0);
        }
#endif
    } catch(const std::exception& e) {
        throw std::runtime_error("Failed to parse chart " + jsonPath.string() + ": " + e.what());
    }
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){ return a.note.timeMs < b.note.timeMs; });
    out.notes.reserve(ordered.size()); for(auto& n:ordered) out.notes.push_back(std::move(n.note));
    return out;
}
}
