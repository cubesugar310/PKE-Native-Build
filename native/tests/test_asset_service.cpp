#include "TestHarness.hpp"
#include "pke/AssetService.hpp"
#include "pke/QuickJsRuntime.hpp"
#include <fstream>
#include <sstream>
#include <chrono>

namespace {
std::string Read(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);std::ostringstream out;out<<in.rdbuf();return out.str();}
void Write(const std::filesystem::path& p,std::string_view text){std::filesystem::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);out<<text;}
void Ok(const pke::QuickJsResult& r){if(!r.ok)throw std::runtime_error(r.error);}
struct Assets {
    std::filesystem::path root=std::filesystem::temp_directory_path()/
        ("pke-cached-assets-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    pke::Config config;
    Assets(){std::filesystem::remove_all(root);config.songsRoot="Songs";config.charactersRoot="Characters";}
    ~Assets(){std::filesystem::remove_all(root);}
    void Song(std::string_view folder,std::string_view chart,std::string_view json){
        Write(root/"Songs"/folder/"Inst.ogg","audio");Write(root/"Songs"/folder/chart,json);
    }
};
constexpr auto sparse=R"({"song":{"song":"Song 2","bpm":150,"speed":1.25,"player2":"characters/DAD.json","notes":[{"mustHitSection":true,"sectionNotes":[[0,0,0],[500,1,100],[1000,4,0]]}]}})";
constexpr auto custom=R"({"name":"Song 10","notes":[{"time":0,"lane":0},{"time":100,"lane":2,"side":"dad"}]})";
}

PKE_TEST("Asset catalog chart metadata matches frozen original async reference cases") {
    pke::QuickJsRuntime vm;Ok(vm.InstallLocaleCompare("en-US"));
    for(const auto* f:{"chart-utils.js","song-difficulty.js","catalog-imports.js","catalog.js"})
        Ok(vm.Evaluate(Read(std::filesystem::path(PKE_COMPAT_DIR)/f),f));
    Ok(vm.Evaluate("const assetReference="+Read(std::filesystem::path(PKE_COMPAT_DIR)/"../tests/fixtures/assets/v112-catalog.json")+";"));
    Ok(vm.Evaluate(R"(
        for(const c of assetReference.charts){
            const got=pkeCatalogChart({text:JSON.stringify(c.input),fallback:c.fallback});
            if(JSON.stringify(got)!==JSON.stringify(c.expected))throw Error('chart reference: '+c.name);
        }
        for(const c of assetReference.search){
            pkeCatalogSetSearch({songs:assetReference.songs});
            const got=pkeCatalogSearch(c.input);
            if(JSON.stringify(got)!==JSON.stringify(c.expected))throw Error('search reference: '+c.name);
        }
    )"));
}

PKE_TEST("Asset catalog skips corrupt preferred charts and isolates unusable songs") {
    Assets a;a.Song("fallback","fallback-hard.json","{broken");a.Song("fallback","chart.json",sparse);
    a.Song("broken","broken-hard.json","null");a.Song("broken","data.json","{}");
    Write(a.root/"Songs"/"missing-audio"/"chart.json",custom);
    pke::AssetService service(a.config,a.root,PKE_COMPAT_DIR,"en-US");const auto c=service.Refresh();
    PKE_REQUIRE(c->songs.size()==1);const auto& s=c->songs.front();
    PKE_REQUIRE(s.folderName=="fallback");PKE_REQUIRE(s.displayName=="Song 2");
    PKE_REQUIRE(s.charts.front().path.filename()=="chart.json");
    PKE_REQUIRE(s.charts.front().playerStars==1);PKE_REQUIRE(s.charts.front().opponentStars==1);
    PKE_REQUIRE(s.charts.front().playerNoteCount==2);PKE_REQUIRE(s.charts.front().opponentNoteCount==1);
    PKE_REQUIRE(s.opponentCharacterId=="dad");PKE_REQUIRE(!c->issues.empty());
}
PKE_TEST("Detected skin IDs retain original JavaScript Unicode lowercase behavior") {
    Assets a;const auto dir=a.root/"Characters"/std::filesystem::u8path("\xC4\xB0" "D");
    Write(dir/"bf.json",R"({"image":"bf","animations":[{"anim":"idle","name":"idle"}]})");
    Write(dir/"bf.xml",R"(<TextureAtlas imagePath="bf.png"><SubTexture name="idle0000" x="0" y="0" width="32" height="32"/></TextureAtlas>)");
    Write(dir/"bf.png","image");pke::AssetService service(a.config,a.root,PKE_COMPAT_DIR,"en-US");
    const auto catalog=service.Refresh();PKE_REQUIRE(catalog->characters.size()==1);
    PKE_REQUIRE(catalog->characters[0].sourceSkinId=="sparrow:i\xCC\x87" "d");
}

PKE_TEST("Asset catalog retains difficulty candidates and original default natural order") {
    Assets a;a.Song("x","x-hard.json",sparse);a.Song("x","x-easy.json",custom);
    a.Song("y","chart.json",custom);
    pke::AssetService service(a.config,a.root,PKE_COMPAT_DIR,"en-US");const auto c=service.Refresh();
    PKE_REQUIRE(c->songs.size()==2);PKE_REQUIRE(c->songs[0].displayName=="Song 2");
    PKE_REQUIRE(c->songs[1].displayName=="Song 10");PKE_REQUIRE(c->songs[0].charts.size()==2);
    PKE_REQUIRE(c->songs[0].charts[0].difficulty=="hard");PKE_REQUIRE(c->songs[0].charts[1].difficulty=="easy");
    PKE_REQUIRE(c->songs[0].charts[0].lightweightJson.find("150")!=std::string::npos);
}

PKE_TEST("Asset catalog filters cached unicode metadata after all source files disappear") {
    Assets a;
    a.Song("wide_name","chart.json",R"({"name":"ＴＥＳＴ カタカナ","notes":[{"time":0,"lane":0}]})");
    a.Song("jack","chart.json",R"({"name":"Hard","notes":[{"time":0,"lane":1},{"time":65,"lane":1},{"time":130,"lane":1},{"time":195,"lane":1},{"time":260,"lane":1},{"time":325,"lane":1},{"time":390,"lane":1},{"time":455,"lane":1},{"time":520,"lane":1},{"time":585,"lane":1},{"time":650,"lane":1},{"time":715,"lane":1},{"time":780,"lane":1}]})");
    pke::AssetService service(a.config,a.root,PKE_COMPAT_DIR,"en-US");const auto old=service.Refresh();
    std::filesystem::remove_all(a.root);
    PKE_REQUIRE(service.FilterSongs("test ｶﾀｶﾅ",pke::Side::Player)==std::vector<std::size_t>{1});
    PKE_REQUIRE(service.FilterSongs("NAME　ＷＩＤＥ",pke::Side::Player)==std::vector<std::size_t>{1});
    PKE_REQUIRE(service.FilterSongs("",pke::Side::Opponent,1)==(std::vector<std::size_t>{0,1}));
    PKE_REQUIRE(service.FilterSongs("",pke::Side::Player,7)==std::vector<std::size_t>{0});
    PKE_REQUIRE(service.Refresh()->songs.empty());PKE_REQUIRE(old->songs.size()==2);
}

PKE_TEST("Asset catalog resolves song icon before character pool and keeps valid characters") {
    Assets a;a.Song("s","chart.json",sparse);
    Write(a.root/"Songs"/"s"/"deep"/"icon-dad.webp","local-icon");
    Write(a.root/"Characters"/"icon-dad.png","global-icon");
    Write(a.root/"Characters"/"bf"/"bf.json",R"({"image":"characters/bf_assets","animations":[{"anim":"idle","name":"idle","fps":24,"loop":true}]})");
    Write(a.root/"Characters"/"bf"/"bf_assets.xml",R"(<TextureAtlas><SubTexture name="idle0000" x="0" y="0" width="64" height="64"/></TextureAtlas>)");
    Write(a.root/"Characters"/"bf"/"bf_assets.png","sprite");
    Write(a.root/"Characters"/"bad.json","{bad");
    pke::AssetService service(a.config,a.root,PKE_COMPAT_DIR,"en-US");const auto c=service.Refresh();
    PKE_REQUIRE(c->songs.front().iconPath==a.root/"Songs"/"s"/"deep"/"icon-dad.webp");
    PKE_REQUIRE(c->characters.size()==1);PKE_REQUIRE(c->characters.front().id=="bf");
    const auto def=c->characters.front().definition;std::filesystem::remove_all(a.root);
    PKE_REQUIRE(def->animations.at("idle").frameIndices.size()==1);PKE_REQUIRE(def->atlas[0].source.w==64);
}

PKE_TEST("Asset catalog resolves cross-directory atlases and prioritizes XML imagePath") {
    Assets a;
    Write(a.root/"Characters"/"defs"/"bf.json",R"({"image":"characters/bf_assets","animations":[{"anim":"idle","name":"idle"}]})");
    Write(a.root/"Characters"/"atlases"/"bf_assets.xml",R"(<TextureAtlas imagePath="actual_sprite.png"><SubTexture name="idle0000" x="0" y="0" width="32" height="32"/></TextureAtlas>)");
    Write(a.root/"Characters"/"atlases"/"actual_sprite.png","actual");
    Write(a.root/"Characters"/"defs"/"bf_assets.png","fallback");
    pke::AssetService service(a.config,a.root,PKE_COMPAT_DIR,"en-US");const auto c=service.Refresh();
    PKE_REQUIRE(c->characters.size()==1);
    PKE_REQUIRE(c->characters[0].definition->imagePath==a.root/"Characters"/"atlases"/"actual_sprite.png");
}

PKE_TEST("Asset catalog prefers the original shorter character definition path") {
    Assets a;
    for(const auto* dir:{"a/deep","z"}){
        const auto d=a.root/"Characters"/dir;
        Write(d/"bf.json",R"({"image":"bf","animations":[{"anim":"idle","name":"idle"}]})");
        Write(d/"bf.xml",R"(<TextureAtlas imagePath="bf.png"><SubTexture name="idle0000" x="0" y="0" width="32" height="32"/></TextureAtlas>)");
        Write(d/"bf.png","sprite");
    }
    pke::AssetService service(a.config,a.root,PKE_COMPAT_DIR,"en-US");const auto c=service.Refresh();
    PKE_REQUIRE(c->characters.size()==1);PKE_REQUIRE(c->characters[0].jsonPath==a.root/"Characters"/"z"/"bf.json");
}

PKE_TEST("Asset catalog excludes character definitions without usable source animations") {
    Assets a;
    Write(a.root/"Characters"/"bf.json",R"({"image":"bf","animations":[]})");
    Write(a.root/"Characters"/"bf.xml",R"(<TextureAtlas imagePath="bf.png"><SubTexture name="idle0000" x="0" y="0" width="32" height="32"/></TextureAtlas>)");
    Write(a.root/"Characters"/"bf.png","sprite");
    pke::AssetService service(a.config,a.root,PKE_COMPAT_DIR,"en-US");
    PKE_REQUIRE(service.Refresh()->characters.empty());
}
