#include "TestHarness.hpp"
#include "pke/LegacySaveImporter.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {
std::string ReadAll(const std::filesystem::path& p){ std::ifstream in(p,std::ios::binary); std::ostringstream ss; ss<<in.rdbuf(); return ss.str(); }
void WriteAll(const std::filesystem::path& p,const std::string& s){ std::filesystem::create_directories(p.parent_path()); std::ofstream out(p,std::ios::binary|std::ios::trunc); out<<s; }
std::filesystem::path TempDir(const char* name){ auto p=std::filesystem::temp_directory_path()/(std::string("pke-native-save-test-")+name); std::error_code ec; std::filesystem::remove_all(p,ec); std::filesystem::create_directories(p); return p; }

const char* kLegacy = R"JSON({
  "format":"pke-beat-battle-full-save",
  "version":1,
  "exportedAt":"2026-10-17T00:00:00.000Z",
  "localStorage":{
    "PKE_BEAT_BATTLE_PLAYER_PROGRESS_CHECKPOINT_V1":"{\"version\":1,\"seasonId\":2,\"updatedAt\":1770000000000,\"completedAt\":1769999999000,\"opponentId\":\"02946\",\"league\":{\"rp\":3845,\"wins\":124,\"losses\":21,\"draws\":2,\"matches\":147,\"onlineSongRangeTier\":\"all\"},\"player\":{\"accuracy\":99.3,\"accuracySum\":14597.1,\"accuracyMatches\":147,\"recentMatches\":[{\"at\":1769999999000}],\"onlineSongRangeTier\":\"all\"},\"title\":{\"peakRp\":3901,\"earned\":[\"ハロwin!\",\"トリックオアビート!\"]},\"h2h\":{\"wins\":3,\"losses\":1,\"draws\":0,\"matches\":4},\"lastOpponentId\":\"02946\"}",
    "pke-beat-battle-user-settings-v1":"{\"version\":1,\"masterVolume\":0.75,\"scrollDirection\":\"up\",\"judgementPosition\":\"center\",\"bindings\":{\"laneLeft\":\"ArrowLeft\",\"laneDown\":\"ArrowDown\",\"laneUp\":\"ArrowUp\",\"laneRight\":\"ArrowRight\"}}",
    "pke-beat-battle-halloween-v1":"{\"version\":1,\"candy\":5123,\"purchasedSkinIds\":[\"skin:a\",\"skin:b\"],\"unlockedTitleIds\":[\"ハロwin!\"],\"claimedRewardIds\":[\"r1\"],\"historyByYear\":{\"2026\":{\"candyEarned\":9000,\"onlineCompleted\":30,\"onlineWins\":20,\"halloweenBossWins\":2,\"purchases\":[\"skin:a\"]}}}",
    "pke-beat-battle-player-name-v1":"かくざとう",
    "pke-beat-battle-player-title-v1":"ハロwin!",
    "pke-beat-battle-title-skin-v1":"psych:impostor-leggi",
    "pke-beat-battle-population-records-v1":"{compressed-or-large-population-placeholder}"
  },
  "indexedDB":{
    "pke-beat-battle-recent-replays-v1":{"replays":[{"id":"keep-me","score":99925}]}
  }
})JSON";
}

PKE_TEST("Legacy Full Save v1 extracts V1.14 checkpoint progress and cosmetics") {
    auto dir=TempDir("parse"); auto src=dir/"legacy.json"; WriteAll(src,kLegacy);
    auto parsed=pke::ReadLegacyFullSave(src);
    PKE_REQUIRE(parsed.valid);
    PKE_REQUIRE(parsed.progress.rp==3845);
    PKE_REQUIRE(parsed.progress.wins==124);
    PKE_REQUIRE(parsed.progress.losses==21);
    PKE_REQUIRE(parsed.progress.draws==2);
    PKE_REQUIRE(parsed.progress.matches==147);
    PKE_REQUIRE_NEAR(parsed.progress.accuracy,99.3,0.0001);
    PKE_REQUIRE(parsed.progress.accuracyMatches==147);
    PKE_REQUIRE(parsed.progress.recentAt==1769999999000ULL);
    PKE_REQUIRE(parsed.progress.peakRp==3901);
    PKE_REQUIRE(parsed.playerName=="かくざとう");
    PKE_REQUIRE(parsed.equippedTitle=="ハロwin!");
    PKE_REQUIRE(parsed.selectedSkin=="psych:impostor-leggi");
    PKE_REQUIRE(parsed.halloweenCandy==5123);
    PKE_REQUIRE(parsed.purchasedSkinIds.size()==2);
    PKE_REQUIRE(parsed.earnedTitleIds.size()==2);
}

PKE_TEST("Legacy importer preserves full old save byte-for-byte and writes separate native summary") {
    auto dir=TempDir("copy"); auto src=dir/"old"/"full-save-v1.json"; WriteAll(src,kLegacy);
    auto saveRoot=dir/"native-save";
    auto result=pke::ImportLegacyFullSave(src,saveRoot);
    PKE_REQUIRE(result.status==pke::LegacyImportStatus::Imported);
    PKE_REQUIRE(std::filesystem::exists(saveRoot/"legacy-full-save-v1.json"));
    PKE_REQUIRE(std::filesystem::exists(saveRoot/"native-save-v1.json"));
    PKE_REQUIRE(ReadAll(src)==ReadAll(saveRoot/"legacy-full-save-v1.json"));
    PKE_REQUIRE(ReadAll(src)==std::string(kLegacy));
    auto native=pke::ReadNativeSaveSummary(saveRoot/"native-save-v1.json");
    PKE_REQUIRE(native.valid);
    PKE_REQUIRE(native.progress.rp==3845);
    PKE_REQUIRE(native.legacyBackupFile=="legacy-full-save-v1.json");
}

PKE_TEST("Legacy importer never replaces an equal or newer native player progress marker") {
    auto dir=TempDir("newer-native"); auto src=dir/"legacy.json"; WriteAll(src,kLegacy); auto root=dir/"save";
    auto first=pke::ImportLegacyFullSave(src,root); PKE_REQUIRE(first.status==pke::LegacyImportStatus::Imported);
    auto newer=pke::ReadNativeSaveSummary(root/"native-save-v1.json");
    newer.progress.matches=200; newer.progress.recentAt=1779999999000ULL; newer.progress.accuracyMatches=200; newer.progress.rp=4100;
    pke::WriteNativeSaveSummary(root/"native-save-v1.json",newer);
    const auto before=ReadAll(root/"native-save-v1.json");
    auto second=pke::ImportLegacyFullSave(src,root);
    PKE_REQUIRE(second.status==pke::LegacyImportStatus::NativeIsNewerOrEqual);
    PKE_REQUIRE(ReadAll(root/"native-save-v1.json")==before);
}

PKE_TEST("Legacy auto discovery prefers explicit path then old native backup location") {
    auto dir=TempDir("discover"); auto exe=dir/"app"; auto local=dir/"local"; std::filesystem::create_directories(exe);
    auto nativeBackup=local/"PKE.BeatBattle.v0.2.42"/"full-save-v1.json"; WriteAll(nativeBackup,kLegacy);
    auto exported=exe/"PKE_Beat_Battle_FULL_SAVE_20261017T000000Z.json"; WriteAll(exported,kLegacy);
    auto c1=pke::DiscoverLegacyFullSaves(exe,local,std::nullopt);
    PKE_REQUIRE(!c1.empty()); PKE_REQUIRE(c1.front()==exported);
    auto explicitPath=dir/"manual.json"; WriteAll(explicitPath,kLegacy);
    auto c2=pke::DiscoverLegacyFullSaves(exe,local,explicitPath);
    PKE_REQUIRE(!c2.empty()); PKE_REQUIRE(c2.front()==explicitPath);
}

PKE_TEST("Invalid file is rejected without creating native save files") {
    auto dir=TempDir("invalid"); auto src=dir/"bad.json"; WriteAll(src,"{\"format\":\"not-pke\",\"version\":1}");
    auto result=pke::ImportLegacyFullSave(src,dir/"save");
    PKE_REQUIRE(result.status==pke::LegacyImportStatus::InvalidLegacySave);
    PKE_REQUIRE(!std::filesystem::exists(dir/"save"/"native-save-v1.json"));
    PKE_REQUIRE(!std::filesystem::exists(dir/"save"/"legacy-full-save-v1.json"));
}

PKE_TEST("Auto import evaluates all discovered saves so stale export cannot beat newer old-native backup") {
    auto dir=TempDir("auto-newest"); auto exe=dir/"app"; auto local=dir/"local"; std::filesystem::create_directories(exe);
    std::string stale=kLegacy;
    auto stalePos=stale.find("\\\"matches\\\":147"); PKE_REQUIRE(stalePos!=std::string::npos); stale.replace(stalePos,std::string("\\\"matches\\\":147").size(),"\\\"matches\\\":100");
    auto staleRp=stale.find("\\\"rp\\\":3845"); PKE_REQUIRE(staleRp!=std::string::npos); stale.replace(staleRp,std::string("\\\"rp\\\":3845").size(),"\\\"rp\\\":3500");
    WriteAll(exe/"PKE_Beat_Battle_FULL_SAVE_20261016T000000Z.json",stale);
    std::string newer=kLegacy;
    auto m=newer.find("\\\"matches\\\":147"); PKE_REQUIRE(m!=std::string::npos); newer.replace(m,std::string("\\\"matches\\\":147").size(),"\\\"matches\\\":180");
    auto rp=newer.find("\\\"rp\\\":3845"); PKE_REQUIRE(rp!=std::string::npos); newer.replace(rp,std::string("\\\"rp\\\":3845").size(),"\\\"rp\\\":4010");
    WriteAll(local/"PKE.BeatBattle.v0.2.42"/"full-save-v1.json",newer);
    auto result=pke::AutoImportLegacyFullSaves(exe,local,std::nullopt);
    PKE_REQUIRE(result.status==pke::LegacyImportStatus::Imported);
    auto native=pke::ReadNativeSaveSummary(pke::DefaultNativeSaveRoot(exe,local)/"native-save-v1.json");
    PKE_REQUIRE(native.valid); PKE_REQUIRE(native.progress.matches==180); PKE_REQUIRE(native.progress.rp==4010);
}

PKE_TEST("Explicit legacy import path does not silently fall back to another save when invalid") {
    auto dir=TempDir("explicit-invalid"); auto exe=dir/"app"; auto local=dir/"local"; std::filesystem::create_directories(exe);
    auto valid=local/"PKE.BeatBattle.v0.2.42"/"full-save-v1.json"; WriteAll(valid,kLegacy);
    auto invalid=dir/"manual.json"; WriteAll(invalid,"{\"format\":\"wrong\"}");
    auto result=pke::AutoImportLegacyFullSaves(exe,local,invalid);
    PKE_REQUIRE(result.status==pke::LegacyImportStatus::InvalidLegacySave);
    PKE_REQUIRE(!std::filesystem::exists(pke::DefaultNativeSaveRoot(exe,local)/"native-save-v1.json"));
}

PKE_TEST("Malformed optional settings do not invalidate recoverable ranked progress") {
    auto dir=TempDir("bad-settings");
    std::string raw=kLegacy;
    const std::string good="{\\\"version\\\":1,\\\"masterVolume\\\":0.75,\\\"scrollDirection\\\":\\\"up\\\",\\\"judgementPosition\\\":\\\"center\\\",\\\"bindings\\\":{\\\"laneLeft\\\":\\\"ArrowLeft\\\",\\\"laneDown\\\":\\\"ArrowDown\\\",\\\"laneUp\\\":\\\"ArrowUp\\\",\\\"laneRight\\\":\\\"ArrowRight\\\"}}";
    const auto pos=raw.find(good); PKE_REQUIRE(pos!=std::string::npos); raw.replace(pos,good.size(),"{broken-settings");
    auto src=dir/"legacy.json"; WriteAll(src,raw);
    auto parsed=pke::ReadLegacyFullSave(src);
    PKE_REQUIRE(parsed.valid);
    PKE_REQUIRE(parsed.progress.rp==3845);
    PKE_REQUIRE(parsed.progress.matches==147);
    PKE_REQUIRE_NEAR(parsed.masterVolume,1.0,0.0001);
}

PKE_TEST("Ranking journal rescues RP and titles when player checkpoint is absent") {
    auto dir=TempDir("journal-fallback"); auto src=dir/"legacy.json";
    const std::string journal=R"({"version":1,"seasonId":2,"updatedAt":1771000000000,"players":{"YOU":{"league":{"rp":3999,"wins":150,"losses":25,"draws":1,"matches":176,"onlineSongRangeTier":"all"},"peakRp":4050,"earned":["ハロwin!","journal-title"]}}})";
    std::ostringstream full;
    full << "{\"format\":\"pke-beat-battle-full-save\",\"version\":1,\"exportedAt\":\"2026-10-17T01:00:00Z\",\"localStorage\":{\"PKE_BEAT_BATTLE_RANKING_JOURNAL_V1\":";
    full << '"'; for(char c:journal){ if(c=='"'||c=='\\') full << '\\'; full << c; } full << '"';
    full << ",\"pke-beat-battle-player-name-v1\":\"かくざとう\"},\"indexedDB\":{}}";
    WriteAll(src,full.str());
    auto parsed=pke::ReadLegacyFullSave(src);
    PKE_REQUIRE(parsed.valid);
    PKE_REQUIRE(parsed.progress.rp==3999);
    PKE_REQUIRE(parsed.progress.matches==176);
    PKE_REQUIRE(parsed.progress.peakRp==4050);
    PKE_REQUIRE(parsed.earnedTitleIds.size()==2);
}
