#include "TestHarness.hpp"
#include "pke/AtomicSaveFile.hpp"
#include "pke/SaveService.hpp"
#include <fstream>
#include <limits>
#include <sstream>

namespace {
std::filesystem::path Temp(const char* name){
    static unsigned sequence=0;
    auto path=std::filesystem::temp_directory_path()/
        (std::string("pke-save-service-")+name+"-"+std::to_string(++sequence));
    std::error_code ec;std::filesystem::remove_all(path,ec);
    std::filesystem::create_directories(path);return path;
}
std::string Read(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);std::ostringstream out;out<<input.rdbuf();return out.str();
}
void Write(const std::filesystem::path& path,std::string_view text){
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path,std::ios::binary|std::ios::trunc);output<<text;
}
pke::NativeSaveSummary Player(){
    pke::NativeSaveSummary player;player.valid=true;player.playerName="かくざとう";
    player.selectedSkin="bf";player.progress.rp=3845;player.progress.wins=124;
    player.progress.losses=21;player.progress.draws=2;player.progress.matches=147;
    player.progress.accuracy=99.3;player.progress.accuracySum=14597.1;
    player.progress.accuracyMatches=147;player.progress.peakRp=3901;
    player.halloweenCandy=5123;player.earnedTitleIds={"ハロwin!"};return player;
}
pke::PopulationCheckpoint Population(){
    return {true,R"({"schema":"pke-native-population-v1","version":1,
      "now":1791989999000,"bootAt":1791989999000,"history":[],"records":{
        "seasonId":2,"seasonLabel":"SEASON 2","seasonStartedAt":1791989999000,"version":12,
        "h2h":{},"skinById":{},"skinMoodById":{},"developmentById":{},"leagueById":{},
        "titleState":{"peakRpById":{},"equippedById":{},"earnedById":{}},
        "player":{"accuracy":93,"accuracySum":0,"accuracyMatches":0,"recentMatches":[],"onlineSongRangeTier":"beginner"},
        "background":{"liveMatches":[],"serial":0,"lastTick":1791989999000,
          "elasticLiveRoomCap":1200,"bossMatches":[],"privateMatches":[],"practiceById":{},
          "spectatorsById":{},"spectatorScanCursor":0},"lastOpponentId":"","seasonResetNoticePending":true}})",
        {},1791989999000};
}
class FaultFiles final:public pke::SaveFileOps {
public:
    enum class Fault {None,CorruptTemporary,Write,Replace};
    Fault fault{Fault::None};
    bool WriteNewAndFlush(const std::filesystem::path& path,std::string_view text) override{
        const bool created=base_->WriteNewAndFlush(path,
            fault==Fault::CorruptTemporary?"{broken":fault==Fault::Write?"{partial":text);
        if(fault==Fault::Write){
            // The writer owns and cleans a partial newly created file on error.
            if(created)base_->Remove(path);
            throw std::runtime_error("injected write failure");
        }
        return created;
    }
    std::string Read(const std::filesystem::path& path) override{return base_->Read(path);}
    void Replace(const std::filesystem::path& temporary,const std::filesystem::path& target) override{
        if(fault==Fault::Replace)throw std::runtime_error("injected interrupted replacement");
        base_->Replace(temporary,target);
    }
    void Remove(const std::filesystem::path& path) noexcept override{base_->Remove(path);}
private:
    std::shared_ptr<pke::SaveFileOps> base_{pke::NativeSaveFileOps()};
};
}

PKE_TEST("Save service distinguishes missing files and preserves malformed saves") {
    const auto root=Temp("load");pke::SaveService saves(root);
    PKE_REQUIRE(saves.LoadPlayer().status==pke::SaveLoadStatus::Missing);
    PKE_REQUIRE(saves.LoadPopulation().status==pke::SaveLoadStatus::Missing);
    Write(root/"native-save-v1.json","{broken-player");
    Write(root/"population-state-v1.json","{broken-population");
    PKE_REQUIRE(saves.LoadPlayer().status==pke::SaveLoadStatus::Invalid);
    PKE_REQUIRE(saves.LoadPopulation().status==pke::SaveLoadStatus::Invalid);
    PKE_REQUIRE(Read(root/"native-save-v1.json")=="{broken-player");
    PKE_REQUIRE(Read(root/"population-state-v1.json")=="{broken-population");
}
PKE_TEST("V114 Halloween state and configured slots survive Native save reload without truncating Candy") {
    auto player=Player();
    pke::ImportHalloweenState(player,R"({"candy":8000000000,"purchasedSkinIds":[" bf ","bf"],"unlockedTitleIds":["trick-or-beat"],"claimedRewardIds":["online|123"],"historyByYear":{"2025":{"candyEarned":9000,"onlineCompleted":12,"onlineWins":10,"halloweenBossWins":2,"purchases":["bf"]},"bad":{}}})");
    pke::ImportHalloweenConfig(player,R"({"skinSlots":[{"skinId":" bf ","price":1234},{"skinId":"missing","price":-1}],"eventOverride":"force-off","bossSongRef":"songs/test"})");
    PKE_REQUIRE(player.halloweenCandy==8000000000ULL);PKE_REQUIRE(player.purchasedSkinIds==std::vector<std::string>{"bf"});
    PKE_REQUIRE(player.halloweenDev.skinSlots[0].price==1234);PKE_REQUIRE(player.halloweenDev.skinSlots[1].price==0);
    PKE_REQUIRE(player.halloweenDev.skinSlots[2].price==3000);PKE_REQUIRE(player.halloweenHistory.size()==1);
    const auto root=Temp("v114-event");pke::SaveService saves(root);PKE_REQUIRE(saves.SavePlayer(player).ok);
    const auto loaded=saves.LoadPlayer();PKE_REQUIRE(loaded.status==pke::SaveLoadStatus::Loaded);
    PKE_REQUIRE(loaded.player.halloweenCandy==8000000000ULL);PKE_REQUIRE(loaded.player.claimedRewardIds==player.claimedRewardIds);
    PKE_REQUIRE(loaded.player.halloweenHistory.at("2025").onlineWins==10);
    PKE_REQUIRE(loaded.player.halloweenDev.skinSlots[0].skinId=="bf");
    PKE_REQUIRE(loaded.player.halloweenDev.bossSongRefs[0]=="songs/test");
    PKE_REQUIRE(loaded.player.halloweenDev.eventOverride=="force-off");
}
PKE_TEST("Upgrading an older Native summary recovers missing event history from the preserved full backup") {
    const auto root=Temp("event-upgrade");auto player=Player();auto old=SerializeNativeSaveSummary(player);
    const auto optional=old.find(",\"claimedRewardIds\"");old.erase(optional,old.find("},\n",optional)-optional);
    const auto dev=old.find("  \"halloweenDev\":");old.erase(dev,old.find('\n',dev)-dev+1);
    Write(root/"native-save-v1.json",old);
    const std::string backup=R"({"format":"pke-beat-battle-full-save","version":1,"localStorage":{"pke-beat-battle-halloween-v1":"{\"candy\":9999,\"claimedRewardIds\":[\"online|old\"],\"historyByYear\":{\"2025\":{\"onlineWins\":12}}}","pke-beat-battle-halloween-dev-v1":"{\"eventOverride\":\"force-off\"}"}})";
    Write(root/"legacy-full-save-v1.json",backup);pke::SaveService saves(root);const auto loaded=saves.LoadPlayer();
    PKE_REQUIRE(loaded.status==pke::SaveLoadStatus::Loaded);PKE_REQUIRE(loaded.player.halloweenCandy==player.halloweenCandy);
    PKE_REQUIRE(loaded.player.progress.rp==player.progress.rp);PKE_REQUIRE(loaded.player.halloweenHistory.at("2025").onlineWins==12);
    PKE_REQUIRE(loaded.player.claimedRewardIds==std::vector<std::string>{"online|old"});
    PKE_REQUIRE(loaded.player.halloweenDev.eventOverride=="force-off");
    PKE_REQUIRE(Read(root/"native-save-v1.json")==old);PKE_REQUIRE(Read(root/"legacy-full-save-v1.json")==backup);
}
PKE_TEST("Rejected but parseable Native destinations survive default-state writes") {
    const auto root=Temp("future-version");pke::SaveService saves(root);
    const std::string player=R"({"format":"pke-native-save","version":2,"player":{"rp":3845}})";
    const std::string population=R"({"schema":"pke-native-population-v1","version":2,"now":1791989999000})";
    Write(root/"native-save-v1.json",player);Write(root/"population-state-v1.json",population);
    PKE_REQUIRE(saves.LoadPlayer().status==pke::SaveLoadStatus::Invalid);PKE_REQUIRE(saves.LoadPopulation().status==pke::SaveLoadStatus::Invalid);
    PKE_REQUIRE(!saves.SavePlayer(Player()).ok);PKE_REQUIRE(!saves.SavePopulation(Population()).ok);
    PKE_REQUIRE(Read(root/"native-save-v1.json")==player);PKE_REQUIRE(Read(root/"population-state-v1.json")==population);
}

PKE_TEST("Save service round trips normalized player and independent population files") {
    const auto root=Temp("roundtrip");pke::SaveService saves(root);
    const auto player=Player();const auto population=Population();
    PKE_REQUIRE(saves.SavePlayer(player).ok);
    PKE_REQUIRE(saves.SavePopulation(population).ok);
    const auto loaded=saves.LoadPlayer();const auto state=saves.LoadPopulation();
    PKE_REQUIRE(loaded.status==pke::SaveLoadStatus::Loaded&&loaded.player.valid);
    PKE_REQUIRE(loaded.player.playerName=="かくざとう"&&loaded.player.selectedSkin=="bf");
    PKE_REQUIRE(loaded.player.progress.rp==3845&&loaded.player.progress.matches==147);
    PKE_REQUIRE(loaded.player.halloweenCandy==5123);
    PKE_REQUIRE(loaded.player.earnedTitleIds==player.earnedTitleIds);
    PKE_REQUIRE(state.status==pke::SaveLoadStatus::Loaded);
    PKE_REQUIRE(state.checkpoint.json==population.json&&state.checkpoint.nowMs==population.nowMs);
    PKE_REQUIRE(saves.SavePopulation(population).ok);
    PKE_REQUIRE(saves.LoadPlayer().player.progress.rp==3845);
}

PKE_TEST("Save service never promotes incomplete temporary files on startup") {
    const auto root=Temp("stale-temporary");pke::SaveService saves(root);
    Write(root/"native-save-v1.json.tmp.crash","{incomplete");
    Write(root/"population-state-v1.json.tmp.crash",Population().json);
    PKE_REQUIRE(saves.LoadPlayer().status==pke::SaveLoadStatus::Missing);
    PKE_REQUIRE(saves.LoadPopulation().status==pke::SaveLoadStatus::Missing);
    PKE_REQUIRE(saves.SavePlayer(Player()).ok);
    PKE_REQUIRE(saves.SavePopulation(Population()).ok);
    PKE_REQUIRE(saves.LoadPlayer().status==pke::SaveLoadStatus::Loaded);
    PKE_REQUIRE(saves.LoadPopulation().status==pke::SaveLoadStatus::Loaded);
    PKE_REQUIRE(Read(root/"native-save-v1.json.tmp.crash")=="{incomplete");
}

PKE_TEST("Save service retains durable player and population after each precommit failure") {
    for(const auto fault:{FaultFiles::Fault::CorruptTemporary,FaultFiles::Fault::Write,FaultFiles::Fault::Replace}){
        const auto root=Temp("fault");auto files=std::make_shared<FaultFiles>();
        pke::SaveService saves(root,files);PKE_REQUIRE(saves.SavePlayer(Player()).ok);
        PKE_REQUIRE(saves.SavePopulation(Population()).ok);
        const auto oldPlayer=Read(root/"native-save-v1.json"),oldPopulation=Read(root/"population-state-v1.json");
        files->fault=fault;
        auto player=Player();player.progress.rp=4100;
        PKE_REQUIRE(!saves.SavePlayer(player).ok);
        PKE_REQUIRE(!saves.SavePopulation(Population()).ok);
        PKE_REQUIRE(Read(root/"native-save-v1.json")==oldPlayer);
        PKE_REQUIRE(Read(root/"population-state-v1.json")==oldPopulation);
        for(const auto& file:std::filesystem::directory_iterator(root))
            PKE_REQUIRE(file.path().filename().string().find(".tmp.")==std::string::npos);
    }
}

PKE_TEST("Save service rejects failed generations and inconsistent population clocks") {
    const auto root=Temp("invalid-population");pke::SaveService saves(root);
    PKE_REQUIRE(saves.SavePopulation(Population()).ok);const auto before=Read(root/"population-state-v1.json");
    auto failed=Population();failed.ok=false;failed.error="failed generation";
    PKE_REQUIRE(!saves.SavePopulation(failed).ok);
    auto wrongClock=Population();wrongClock.nowMs++;
    PKE_REQUIRE(!saves.SavePopulation(wrongClock).ok);
    for(const auto* invalid:{"{}","[]","{\"schema\":\"pke-native-population-v1\",\"version\":1}"}){
        auto checkpoint=Population();checkpoint.json=invalid;
        PKE_REQUIRE(!saves.SavePopulation(checkpoint).ok);
    }
    auto invalid=Population();const auto position=invalid.json.find("\"skinById\":{}");
    PKE_REQUIRE(position!=std::string::npos);invalid.json.replace(position,13,"\"skinById\":[]");
    PKE_REQUIRE(!saves.SavePopulation(invalid).ok);
    PKE_REQUIRE(Read(root/"population-state-v1.json")==before);
}

PKE_TEST("Save service rejects invalid player numbers before replacing a good save") {
    const auto root=Temp("invalid-player");pke::SaveService saves(root);
    PKE_REQUIRE(saves.SavePlayer(Player()).ok);const auto before=Read(root/"native-save-v1.json");
    auto bad=Player();bad.progress.accuracy=std::numeric_limits<double>::quiet_NaN();
    PKE_REQUIRE(!saves.SavePlayer(bad).ok);
    bad=Player();bad.progress.rp=-1;PKE_REQUIRE(!saves.SavePlayer(bad).ok);
    bad=Player();bad.masterVolume=2;PKE_REQUIRE(!saves.SavePlayer(bad).ok);
    PKE_REQUIRE(Read(root/"native-save-v1.json")==before);
}

PKE_TEST("Legacy Native reader rejects incomplete or string-coerced player saves") {
    const auto root=Temp("strict-native-reader");const auto path=root/"native-save-v1.json";
    Write(path,R"({"format":"pke-native-save","version":1})");
    PKE_REQUIRE(!pke::ReadNativeSaveSummary(path).valid);
    auto json=pke::SerializeNativeSaveSummary(Player());
    const std::string field="\"rp\":3845";const auto position=json.find(field);
    PKE_REQUIRE(position!=std::string::npos);json.replace(position,field.size(),"\"rp\":\"3845\"");
    Write(path,json);PKE_REQUIRE(!pke::ReadNativeSaveSummary(path).valid);
    PKE_REQUIRE(pke::SaveService(root).LoadPlayer().status==pke::SaveLoadStatus::Invalid);
}

PKE_TEST("Save service reports a real unwritable destination without changing the blocker") {
    const auto root=Temp("write-failure");Write(root/"blocker","keep");
    pke::SaveService saves(root/"blocker"/"save");
    PKE_REQUIRE(!saves.SavePlayer(Player()).ok);
    PKE_REQUIRE(!saves.SavePopulation(Population()).ok);
    PKE_REQUIRE(Read(root/"blocker")=="keep");
}

PKE_TEST("Native save updates leave legacy migration source and full backup byte identical") {
    const auto root=Temp("legacy");
    const auto legacy=root/"legacy"/"full-save-v1.json";
    const std::string bytes=R"({"format":"pke-beat-battle-full-save","version":1,"exportedAt":"2026-10-17T00:00:00Z","localStorage":{},"indexedDB":{"keep":"unchanged"}})";
    Write(legacy,bytes);
    const auto target=root/"native";
    PKE_REQUIRE(pke::ImportLegacyFullSave(legacy,target).status==pke::LegacyImportStatus::Imported);
    pke::SaveService saves(target);PKE_REQUIRE(saves.SavePlayer(Player()).ok);
    PKE_REQUIRE(saves.SavePopulation(Population()).ok);
    PKE_REQUIRE(Read(legacy)==bytes);
    PKE_REQUIRE(Read(target/"legacy-full-save-v1.json")==bytes);
}

PKE_TEST("Legacy migration refuses a source aliasing a Native durable destination") {
    const auto root=Temp("source-alias");
    const std::string bytes=R"({"format":"pke-beat-battle-full-save","version":1,"localStorage":{}})";
    for(const auto* name:{"native-save-v1.json","population-state-v1.json"}){
        const auto source=root/name;Write(source,bytes);
        const auto result=pke::ImportLegacyFullSave(source,root);
        PKE_REQUIRE(result.status==pke::LegacyImportStatus::WriteFailed);
        PKE_REQUIRE(Read(source)==bytes);
    }
}

PKE_TEST("Legacy backup can be an import source without rewriting that source") {
    const auto root=Temp("backup-source");const auto source=root/"legacy-full-save-v1.json";
    const std::string bytes=R"({"format":"pke-beat-battle-full-save","version":1,"localStorage":{}})";
    Write(source,bytes);const auto timestamp=std::filesystem::last_write_time(source);
    PKE_REQUIRE(pke::ImportLegacyFullSave(source,root).status==pke::LegacyImportStatus::Imported);
    PKE_REQUIRE(Read(source)==bytes);
    PKE_REQUIRE(std::filesystem::last_write_time(source)==timestamp);
}

PKE_TEST("Save service refuses legacy-format files at Native destination names") {
    const auto root=Temp("protect-legacy");pke::SaveService saves(root);
    const std::string bytes=R"({"format":"pke-beat-battle-full-save","version":1,"localStorage":{}})";
    Write(root/"native-save-v1.json",bytes);Write(root/"population-state-v1.json",bytes);
    PKE_REQUIRE(!saves.SavePlayer(Player()).ok);
    PKE_REQUIRE(!saves.SavePopulation(Population()).ok);
    PKE_REQUIRE(Read(root/"native-save-v1.json")==bytes);
    PKE_REQUIRE(Read(root/"population-state-v1.json")==bytes);
}

PKE_TEST("Save service preserves legacy data when its nesting exceeds Native validation limits") {
    const auto root=Temp("deep-legacy");pke::SaveService saves(root);
    const std::string bytes=std::string(R"({"format":"pke-beat-battle-full-save","version":1,"localStorage":{},"indexedDB":)")+
        std::string(65,'[')+"0"+std::string(65,']')+"}";
    Write(root/"native-save-v1.json",bytes);Write(root/"population-state-v1.json",bytes);
    PKE_REQUIRE(pke::ReadLegacyFullSave(root/"native-save-v1.json").valid);
    PKE_REQUIRE(!saves.SavePlayer(Player()).ok);
    PKE_REQUIRE(!saves.SavePopulation(Population()).ok);
    PKE_REQUIRE(Read(root/"native-save-v1.json")==bytes);
    PKE_REQUIRE(Read(root/"population-state-v1.json")==bytes);
}

PKE_TEST("Save service rejects LIVE rooms that cannot expire or identify their participants") {
    const auto root=Temp("malformed-live");pke::SaveService saves(root);
    PKE_REQUIRE(saves.SavePopulation(Population()).ok);
    const auto before=Read(root/"population-state-v1.json");
    for(const auto* room:{
        R"({"aId":"00001","bId":"00002"})",
        R"({"id":"","aId":"00001","bId":"00002","startedAt":1791989999000,"endsAt":1791990009000})",
        R"({"id":"room-1","aId":"00001","bId":"00002","startedAt":1791989999000,"endsAt":1791989999000})",
        R"({"id":"room-1","aId":"00001","bId":"00001","startedAt":1791989999000,"endsAt":1791990009000})"}){
        auto checkpoint=Population();const std::string field="\"liveMatches\":[]";
        const auto position=checkpoint.json.find(field);PKE_REQUIRE(position!=std::string::npos);
        checkpoint.json.replace(position,field.size(),"\"liveMatches\":["+std::string(room)+"]");
        PKE_REQUIRE(!saves.SavePopulation(checkpoint).ok);
        PKE_REQUIRE(Read(root/"population-state-v1.json")==before);
    }
}

PKE_TEST("Save service retains valid scientific notation player progress during normalization") {
    const auto root=Temp("scientific-player");pke::SaveService saves(root);
    auto json=pke::SerializeNativeSaveSummary(Player());
    for(const auto& pair:{std::pair{"\"rp\":3845","\"rp\":3.845e3"},
                          std::pair{"\"matches\":147","\"matches\":1.47e2"}}){
        const auto position=json.find(pair.first);PKE_REQUIRE(position!=std::string::npos);
        json.replace(position,std::string(pair.first).size(),pair.second);
    }
    Write(root/"native-save-v1.json",json);
    const auto loaded=saves.LoadPlayer();PKE_REQUIRE(loaded.status==pke::SaveLoadStatus::Loaded);
    PKE_REQUIRE(loaded.player.progress.rp==3845&&loaded.player.progress.matches==147);
    const auto legacyEntry=pke::ReadNativeSaveSummary(root/"native-save-v1.json");
    PKE_REQUIRE(legacyEntry.valid&&legacyEntry.progress.rp==3845&&legacyEntry.progress.matches==147);
}
