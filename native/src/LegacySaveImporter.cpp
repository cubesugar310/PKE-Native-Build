#include "pke/LegacySaveImporter.hpp"
#include "pke/SaveService.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef PKE_NATIVE_USE_NLOHMANN
#include <nlohmann/json.hpp>
#else
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#endif

namespace pke {
namespace {
constexpr const char* kLegacyFormat = "pke-beat-battle-full-save";
constexpr int kLegacyVersion = 1;
constexpr const char* kNativeFormat = "pke-native-save";
constexpr int kNativeVersion = 1;
constexpr const char* kCheckpointKey = "PKE_BEAT_BATTLE_PLAYER_PROGRESS_CHECKPOINT_V1";
constexpr const char* kRankingJournalKey = "PKE_BEAT_BATTLE_RANKING_JOURNAL_V1";
constexpr const char* kSettingsKey = "pke-beat-battle-user-settings-v1";
constexpr const char* kHalloweenKey = "pke-beat-battle-halloween-v1";
constexpr const char* kPlayerNameKey = "pke-beat-battle-player-name-v1";
constexpr const char* kPlayerTitleKey = "pke-beat-battle-player-title-v1";
constexpr const char* kTitleSkinKey = "pke-beat-battle-title-skin-v1";

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open save file");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string JsonEscape(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    return out.str();
}

std::string StringArrayJson(const std::vector<std::string>& values) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << '"' << JsonEscape(values[i]) << '"';
    }
    out << ']';
    return out.str();
}

std::string NowIsoUtc() {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::uint64_t ClampUInt64(long long value) {
    return value < 0 ? 0ULL : static_cast<std::uint64_t>(value);
}

int ClampInt(long long value) {
    if (value < 0) return 0;
    if (value > static_cast<long long>(std::numeric_limits<int>::max())) return std::numeric_limits<int>::max();
    return static_cast<int>(value);
}

std::string NormalizeRange(std::string value) {
    if (value == "all" || value == "standard") return value;
    return "beginner";
}

#ifdef PKE_NATIVE_USE_NLOHMANN
using Json = nlohmann::json;
Json ParseJson(const std::string& raw) { return Json::parse(raw); }
std::string StorageString(const Json& root, const char* key) {
    if (!root.contains("localStorage") || !root["localStorage"].is_object()) return {};
    const auto& ls = root["localStorage"];
    if (!ls.contains(key) || !ls[key].is_string()) return {};
    return ls[key].get<std::string>();
}
std::vector<std::string> StringVector(const Json& node, const char* key) {
    std::vector<std::string> out;
    if (!node.is_object() || !node.contains(key) || !node[key].is_array()) return out;
    for (const auto& v : node[key]) if (v.is_string()) out.push_back(v.get<std::string>());
    return out;
}
#else
using Json = boost::property_tree::ptree;
Json ParseJson(const std::string& raw) { Json j; std::istringstream in(raw); boost::property_tree::read_json(in, j); return j; }
std::string StorageString(const Json& root, const char* key) {
    auto ls = root.get_child_optional("localStorage");
    if (!ls) return {};
    return ls->get<std::string>(key, "");
}
std::vector<std::string> StringVector(const Json& node, const char* key) {
    std::vector<std::string> out;
    auto arr = node.get_child_optional(key);
    if (!arr) return out;
    for (const auto& item : *arr) out.push_back(item.second.get_value<std::string>());
    return out;
}
#endif

bool FillProgressFromCheckpoint(LegacyFullSaveData& out, const std::string& raw) {
    if (raw.empty()) return false;
    const auto cp = ParseJson(raw);
#ifdef PKE_NATIVE_USE_NLOHMANN
    const auto league = cp.value("league", Json::object());
    const auto player = cp.value("player", Json::object());
    const auto title = cp.value("title", Json::object());
    out.progress.rp = ClampInt(league.value("rp", 0LL));
    out.progress.wins = ClampInt(league.value("wins", 0LL));
    out.progress.losses = ClampInt(league.value("losses", 0LL));
    out.progress.draws = ClampInt(league.value("draws", 0LL));
    const auto declaredMatches = ClampUInt64(league.value("matches", 0LL));
    const auto resultMatches = static_cast<std::uint64_t>(out.progress.wins) + static_cast<std::uint64_t>(out.progress.losses) + static_cast<std::uint64_t>(out.progress.draws);
    out.progress.matches = std::max(declaredMatches, resultMatches);
    out.progress.accuracy = std::clamp(player.value("accuracy", 0.0), 0.0, 100.0);
    out.progress.accuracySum = std::max(0.0, player.value("accuracySum", 0.0));
    out.progress.accuracyMatches = ClampUInt64(player.value("accuracyMatches", 0LL));
    out.progress.recentAt = ClampUInt64(cp.value("completedAt", 0LL));
    if (player.contains("recentMatches") && player["recentMatches"].is_array()) for (const auto& m : player["recentMatches"]) if (m.is_object()) out.progress.recentAt = std::max(out.progress.recentAt, ClampUInt64(m.value("at", 0LL)));
    out.progress.peakRp = ClampInt(title.value("peakRp", 0LL));
    out.progress.onlineSongRangeTier = NormalizeRange(player.value("onlineSongRangeTier", league.value("onlineSongRangeTier", std::string("beginner"))));
    out.progress.lastOpponentId = cp.value("lastOpponentId", std::string{});
    out.earnedTitleIds = StringVector(title, "earned");
#else
    auto league = cp.get_child_optional("league"); auto player = cp.get_child_optional("player"); auto title = cp.get_child_optional("title");
    if (league) {
        out.progress.rp=ClampInt(league->get<long long>("rp",0)); out.progress.wins=ClampInt(league->get<long long>("wins",0)); out.progress.losses=ClampInt(league->get<long long>("losses",0)); out.progress.draws=ClampInt(league->get<long long>("draws",0));
        const auto declared=ClampUInt64(league->get<long long>("matches",0)); const auto results=static_cast<std::uint64_t>(out.progress.wins)+static_cast<std::uint64_t>(out.progress.losses)+static_cast<std::uint64_t>(out.progress.draws); out.progress.matches=std::max(declared,results);
        out.progress.onlineSongRangeTier=NormalizeRange(league->get<std::string>("onlineSongRangeTier","beginner"));
    }
    if (player) {
        out.progress.accuracy=std::clamp(player->get<double>("accuracy",0.0),0.0,100.0); out.progress.accuracySum=std::max(0.0,player->get<double>("accuracySum",0.0)); out.progress.accuracyMatches=ClampUInt64(player->get<long long>("accuracyMatches",0));
        out.progress.onlineSongRangeTier=NormalizeRange(player->get<std::string>("onlineSongRangeTier",out.progress.onlineSongRangeTier));
        if(auto recent=player->get_child_optional("recentMatches")) for(const auto& m:*recent) out.progress.recentAt=std::max(out.progress.recentAt,ClampUInt64(m.second.get<long long>("at",0)));
    }
    out.progress.recentAt=std::max(out.progress.recentAt,ClampUInt64(cp.get<long long>("completedAt",0)));
    if(title){out.progress.peakRp=ClampInt(title->get<long long>("peakRp",0));out.earnedTitleIds=StringVector(*title,"earned");}
    out.progress.lastOpponentId=cp.get<std::string>("lastOpponentId","");
#endif
    return true;
}

bool FillProgressFromRankingJournal(LegacyFullSaveData& out, const std::string& raw) {
    if(raw.empty()) return false;
    const auto journal=ParseJson(raw);
#ifdef PKE_NATIVE_USE_NLOHMANN
    if(!journal.is_object()||!journal.contains("players")||!journal["players"].is_object()||!journal["players"].contains("YOU")||!journal["players"]["YOU"].is_object()) return false;
    const auto& you=journal["players"]["YOU"];
    if(!you.contains("league")||!you["league"].is_object()) return false;
    const auto& league=you["league"];
    out.progress.rp=ClampInt(league.value("rp",0LL)); out.progress.wins=ClampInt(league.value("wins",0LL)); out.progress.losses=ClampInt(league.value("losses",0LL)); out.progress.draws=ClampInt(league.value("draws",0LL));
    const auto declared=ClampUInt64(league.value("matches",0LL)); const auto results=static_cast<std::uint64_t>(out.progress.wins)+static_cast<std::uint64_t>(out.progress.losses)+static_cast<std::uint64_t>(out.progress.draws); out.progress.matches=std::max(declared,results);
    out.progress.onlineSongRangeTier=NormalizeRange(league.value("onlineSongRangeTier",std::string("beginner"))); out.progress.peakRp=ClampInt(you.value("peakRp",0LL)); out.earnedTitleIds=StringVector(you,"earned"); out.progress.recentAt=ClampUInt64(journal.value("updatedAt",0LL));
#else
    auto players=journal.get_child_optional("players"); if(!players) return false; auto you=players->get_child_optional("YOU"); if(!you) return false; auto league=you->get_child_optional("league"); if(!league) return false;
    out.progress.rp=ClampInt(league->get<long long>("rp",0)); out.progress.wins=ClampInt(league->get<long long>("wins",0)); out.progress.losses=ClampInt(league->get<long long>("losses",0)); out.progress.draws=ClampInt(league->get<long long>("draws",0));
    const auto declared=ClampUInt64(league->get<long long>("matches",0)); const auto results=static_cast<std::uint64_t>(out.progress.wins)+static_cast<std::uint64_t>(out.progress.losses)+static_cast<std::uint64_t>(out.progress.draws); out.progress.matches=std::max(declared,results);
    out.progress.onlineSongRangeTier=NormalizeRange(league->get<std::string>("onlineSongRangeTier","beginner")); out.progress.peakRp=ClampInt(you->get<long long>("peakRp",0)); out.earnedTitleIds=StringVector(*you,"earned"); out.progress.recentAt=ClampUInt64(journal.get<long long>("updatedAt",0));
#endif
    return true;
}

void FillSettings(LegacyFullSaveData& out, const std::string& raw) {
    if (raw.empty()) return;
    const auto j = ParseJson(raw);
#ifdef PKE_NATIVE_USE_NLOHMANN
    out.masterVolume=std::clamp(j.value("masterVolume",1.0),0.0,1.0);
    out.scrollDirection=j.value("scrollDirection",std::string("up"))=="down"?"down":"up";
    out.judgementPosition=j.value("judgementPosition",std::string("center"))=="corner"?"corner":"center";
#else
    out.masterVolume=std::clamp(j.get<double>("masterVolume",1.0),0.0,1.0);
    out.scrollDirection=j.get<std::string>("scrollDirection","up")=="down"?"down":"up";
    out.judgementPosition=j.get<std::string>("judgementPosition","center")=="corner"?"corner":"center";
#endif
}

void FillHalloween(LegacyFullSaveData& out, const std::string& raw) {
    ImportHalloweenState(out,raw);
}

NativeSaveSummary ToNative(const LegacyFullSaveData& legacy) {
    NativeSaveSummary out;
    out.valid=true; out.version=kNativeVersion; out.importedAt=NowIsoUtc(); out.legacyExportedAt=legacy.exportedAt; out.legacySourcePath=legacy.sourcePath.string(); out.progress=legacy.progress;
    out.playerName=legacy.playerName; out.equippedTitle=legacy.equippedTitle; out.selectedSkin=legacy.selectedSkin; out.masterVolume=legacy.masterVolume; out.scrollDirection=legacy.scrollDirection; out.judgementPosition=legacy.judgementPosition;
    static_cast<HalloweenData&>(out)=legacy;out.earnedTitleIds=legacy.earnedTitleIds;
    return out;
}

std::string SerializeNative(const NativeSaveSummary& s) {
    std::ostringstream history,config;history<<'{';bool first=true;
    for(const auto& [year,h]:s.halloweenHistory){if(!first)history<<',';first=false;
        history<<'"'<<JsonEscape(year)<<"\":{\"candyEarned\":"<<h.candyEarned<<",\"onlineCompleted\":"<<h.onlineCompleted
            <<",\"onlineWins\":"<<h.onlineWins<<",\"halloweenBossWins\":"<<h.halloweenBossWins<<",\"purchases\":"<<StringArrayJson(h.purchases)<<'}';}history<<'}';
    config<<"{\"version\":2,\"eventOverride\":\""<<JsonEscape(s.halloweenDev.eventOverride)<<"\",\"bossSongRefs\":[";
    for(std::size_t i=0;i<3;++i){if(i)config<<',';config<<'"'<<JsonEscape(s.halloweenDev.bossSongRefs[i])<<'"';}config<<"],\"skinSlots\":[";
    for(std::size_t i=0;i<4;++i){if(i)config<<',';const auto& slot=s.halloweenDev.skinSlots[i];
        config<<"{\"skinId\":\""<<JsonEscape(slot.skinId)<<"\",\"price\":"<<slot.price<<'}';}config<<"]}";
    std::ostringstream out;
    out << "{\n"
        << "  \"format\":\"" << kNativeFormat << "\",\n"
        << "  \"version\":" << kNativeVersion << ",\n"
        << "  \"importedAt\":\"" << JsonEscape(s.importedAt) << "\",\n"
        << "  \"legacyExportedAt\":\"" << JsonEscape(s.legacyExportedAt) << "\",\n"
        << "  \"legacyBackupFile\":\"" << JsonEscape(s.legacyBackupFile) << "\",\n"
        << "  \"legacySourcePath\":\"" << JsonEscape(s.legacySourcePath) << "\",\n"
        << "  \"player\":{\n"
        << "    \"rp\":" << s.progress.rp << ",\"wins\":" << s.progress.wins << ",\"losses\":" << s.progress.losses << ",\"draws\":" << s.progress.draws << ",\"matches\":" << s.progress.matches << ",\n"
        << "    \"accuracy\":" << std::setprecision(15) << s.progress.accuracy << ",\"accuracySum\":" << s.progress.accuracySum << ",\"accuracyMatches\":" << s.progress.accuracyMatches << ",\"recentAt\":" << s.progress.recentAt << ",\n"
        << "    \"peakRp\":" << s.progress.peakRp << ",\"onlineSongRangeTier\":\"" << JsonEscape(s.progress.onlineSongRangeTier) << "\",\"lastOpponentId\":\"" << JsonEscape(s.progress.lastOpponentId) << "\"\n"
        << "  },\n"
        << "  \"playerName\":\"" << JsonEscape(s.playerName) << "\",\n"
        << "  \"equippedTitle\":\"" << JsonEscape(s.equippedTitle) << "\",\n"
        << "  \"selectedSkin\":\"" << JsonEscape(s.selectedSkin) << "\",\n"
        << "  \"settings\":{\"masterVolume\":" << s.masterVolume << ",\"scrollDirection\":\"" << JsonEscape(s.scrollDirection) << "\",\"judgementPosition\":\"" << JsonEscape(s.judgementPosition) << "\"},\n"
        << "  \"halloween\":{\"candy\":" << s.halloweenCandy << ",\"purchasedSkinIds\":" << StringArrayJson(s.purchasedSkinIds) << ",\"unlockedTitleIds\":" << StringArrayJson(s.unlockedTitleIds)
        << ",\"claimedRewardIds\":"<<StringArrayJson(s.claimedRewardIds)<<",\"historyByYear\":"<<history.str()<<"},\n"
        << "  \"halloweenDev\":"<<config.str()<<",\n"
        << "  \"earnedTitleIds\":" << StringArrayJson(s.earnedTitleIds) << "\n"
        << "}\n";
    return out.str();
}

bool SameFile(const std::filesystem::path& left,const std::filesystem::path& right){
    std::error_code ec;
    if(std::filesystem::equivalent(left,right,ec))return true;
    const auto a=std::filesystem::weakly_canonical(left),b=std::filesystem::weakly_canonical(right);
    return a==b;
}
void ValidateLegacyBackup(std::string_view text){
    const auto root=ParseJson(std::string(text));
#ifdef PKE_NATIVE_USE_NLOHMANN
    if(!root.is_object()||root.value("format",std::string{})!=kLegacyFormat||
       root.value("version",0)!=kLegacyVersion||!root.contains("localStorage")||!root["localStorage"].is_object())
#else
    if(root.get<std::string>("format","")!=kLegacyFormat||root.get<int>("version",0)!=kLegacyVersion||
       !root.get_child_optional("localStorage"))
#endif
        throw std::runtime_error("Invalid legacy backup bytes");
}

} // namespace

PlayerProgressMarker ProgressMarkerFor(const ImportedPlayerProgress& p) { return {p.matches,p.recentAt,p.accuracyMatches}; }

int CompareProgressMarkers(const PlayerProgressMarker& left, const PlayerProgressMarker& right) {
    if(left.matches!=right.matches) return left.matches>right.matches?1:-1;
    if(left.recentAt!=right.recentAt) return left.recentAt>right.recentAt?1:-1;
    if(left.accuracyMatches!=right.accuracyMatches) return left.accuracyMatches>right.accuracyMatches?1:-1;
    return 0;
}

LegacyFullSaveData ReadLegacyFullSave(const std::filesystem::path& path) {
    LegacyFullSaveData out; out.sourcePath=path;
    try {
        const auto root=ParseJson(ReadAll(path));
#ifdef PKE_NATIVE_USE_NLOHMANN
        if(!root.is_object()||root.value("format",std::string{})!=kLegacyFormat||root.value("version",0)!=kLegacyVersion||!root.contains("localStorage")||!root["localStorage"].is_object()) return out;
        out.exportedAt=root.value("exportedAt",std::string{});
#else
        if(root.get<std::string>("format","")!=kLegacyFormat||root.get<int>("version",0)!=kLegacyVersion||!root.get_child_optional("localStorage")) return out;
        out.exportedAt=root.get<std::string>("exportedAt","");
#endif
        bool progressRecovered=false;
        try { progressRecovered=FillProgressFromCheckpoint(out,StorageString(root,kCheckpointKey)); } catch(...) {}
        if(!progressRecovered) { try { progressRecovered=FillProgressFromRankingJournal(out,StorageString(root,kRankingJournalKey)); } catch(...) {} }
        try { FillSettings(out,StorageString(root,kSettingsKey)); } catch(...) {}
        try { FillHalloween(out,StorageString(root,kHalloweenKey)); } catch(...) {}
        try { ImportHalloweenConfig(out,StorageString(root,"pke-beat-battle-halloween-dev-v1")); } catch(...) {}
        out.playerName=StorageString(root,kPlayerNameKey); out.equippedTitle=StorageString(root,kPlayerTitleKey); out.selectedSkin=StorageString(root,kTitleSkinKey);
        out.valid=true;
    } catch(...) { out.valid=false; }
    return out;
}

NativeSaveSummary ParseNativeSaveSummary(const std::string& json) {
    try{return DecodeNativePlayerSave(json);}catch(...){return {};}
}

NativeSaveSummary ReadNativeSaveSummary(const std::filesystem::path& path) {
    try{return ParseNativeSaveSummary(ReadAll(path));}
    catch(...){return {};}
}
std::string SerializeNativeSaveSummary(const NativeSaveSummary& summary){return SerializeNative(summary);}
void WriteNativeSaveSummary(const std::filesystem::path& path, const NativeSaveSummary& summary) {
    if(ReadLegacyFullSave(path).valid)throw std::runtime_error("Refusing to replace a legacy migration source");
    WriteValidatedSaveFile(path,SerializeNativeSaveSummary(summary),ValidateNativePlayerSave);
}

std::vector<std::filesystem::path> DiscoverLegacyFullSaves(const std::filesystem::path& executableDir,const std::filesystem::path& localAppData,const std::optional<std::filesystem::path>& explicitPath) {
    std::vector<std::filesystem::path> out;
    auto add=[&](const std::filesystem::path& p){ if(p.empty()||!std::filesystem::is_regular_file(p))return; auto n=p.lexically_normal(); if(std::find(out.begin(),out.end(),n)==out.end())out.push_back(n); };
    if(explicitPath) add(*explicitPath);
    std::vector<std::pair<std::filesystem::file_time_type,std::filesystem::path>> exports;
    std::error_code ec;
    if(std::filesystem::is_directory(executableDir,ec)) for(const auto& e:std::filesystem::directory_iterator(executableDir,ec)) if(e.is_regular_file(ec)){const auto name=e.path().filename().string();if(name.rfind("PKE_Beat_Battle_FULL_SAVE_",0)==0&&e.path().extension()==".json")exports.push_back({e.last_write_time(ec),e.path()});}
    std::sort(exports.begin(),exports.end(),[](const auto& a,const auto& b){return a.first>b.first;}); for(const auto& e:exports)add(e.second);
    add(executableDir/"full-save-v1.json");
    if(!localAppData.empty()) { add(localAppData/"PKE.BeatBattle.v0.2.42"/"full-save-v1.json"); add(localAppData/"PKE Beat Battle"/"full-save-v1.json"); add(localAppData/"PKEBeatBattle"/"full-save-v1.json"); }
    return out;
}

std::filesystem::path DefaultNativeSaveRoot(const std::filesystem::path& executableDir,const std::filesystem::path& localAppData) { return localAppData.empty()?(executableDir/"Save"):(localAppData/"PKE.Native.BeatBattle"); }

LegacyImportResult ImportLegacyFullSave(const std::filesystem::path& sourcePath,const std::filesystem::path& nativeSaveRoot) {
    LegacyImportResult result; result.sourcePath=sourcePath; result.nativeSavePath=nativeSaveRoot/"native-save-v1.json";
    if(!std::filesystem::is_regular_file(sourcePath)){result.status=LegacyImportStatus::NoLegacySave;result.message="Legacy Full Save not found";return result;}
    const auto legacy=ReadLegacyFullSave(sourcePath); if(!legacy.valid){result.status=LegacyImportStatus::InvalidLegacySave;result.message="Legacy Full Save is invalid or unsupported";return result;}
    const auto existing=ReadNativeSaveSummary(result.nativeSavePath); if(existing.valid&&CompareProgressMarkers(ProgressMarkerFor(existing.progress),ProgressMarkerFor(legacy.progress))>=0){result.status=LegacyImportStatus::NativeIsNewerOrEqual;result.message="Native player progress is newer or equal; legacy save left untouched";return result;}
    try {
        if(SameFile(sourcePath,result.nativeSavePath)||SameFile(sourcePath,nativeSaveRoot/"population-state-v1.json"))
            throw std::runtime_error("Legacy migration source aliases a Native save destination");
        std::filesystem::create_directories(nativeSaveRoot);
        const auto raw=ReadAll(sourcePath);const auto backup=nativeSaveRoot/"legacy-full-save-v1.json";
        if(!SameFile(sourcePath,backup))WriteValidatedSaveFile(backup,raw,ValidateLegacyBackup);
        auto summary=ToNative(legacy); WriteNativeSaveSummary(result.nativeSavePath,summary);
        result.status=LegacyImportStatus::Imported; result.message="Legacy PKE save imported without modifying the source"; return result;
    } catch(const std::exception& e) { result.status=LegacyImportStatus::WriteFailed; result.message=e.what(); return result; }
}


LegacyImportResult AutoImportLegacyFullSaves(const std::filesystem::path& executableDir,const std::filesystem::path& localAppData,const std::optional<std::filesystem::path>& explicitPath) {
    const auto root=DefaultNativeSaveRoot(executableDir,localAppData);
    if(explicitPath) return ImportLegacyFullSave(*explicitPath,root);
    const auto candidates=DiscoverLegacyFullSaves(executableDir,localAppData,std::nullopt);
    LegacyImportResult best; best.status=LegacyImportStatus::NoLegacySave; best.nativeSavePath=root/"native-save-v1.json"; best.message="No legacy PKE Full Save discovered";
    for(const auto& candidate:candidates){
        const auto current=ImportLegacyFullSave(candidate,root);
        if(current.status==LegacyImportStatus::Imported) best=current;
        else if(best.status==LegacyImportStatus::NoLegacySave && current.status==LegacyImportStatus::NativeIsNewerOrEqual) best=current;
    }
    return best;
}

} // namespace pke
