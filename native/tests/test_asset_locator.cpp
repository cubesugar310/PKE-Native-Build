#include "TestHarness.hpp"
#include "pke/AssetLocator.hpp"
#include <filesystem>
#include <fstream>

namespace { void touch(const std::filesystem::path& p){ std::filesystem::create_directories(p.parent_path()); std::ofstream(p).put('\n'); } }

PKE_TEST("AssetLocator resolves relative roots against executable directory") {
    auto root=std::filesystem::temp_directory_path()/"pke-asset-relative"; std::filesystem::remove_all(root);
    pke::Config cfg; cfg.songsRoot="Songs"; cfg.charactersRoot="Characters";
    pke::AssetLocator loc(cfg, root);
    touch(root/"Songs"/"pinkwave"/"Inst.ogg");
    PKE_REQUIRE(loc.ResolveSongFolder("pinkwave") == (root/"Songs"/"pinkwave").lexically_normal());
}

PKE_TEST("AssetLocator preserves absolute roots and Voices is optional") {
    auto root=std::filesystem::temp_directory_path()/"pke-asset-absolute"; std::filesystem::remove_all(root);
    auto songs=root/"songs"; auto chars=root/"chars"; pke::Config cfg; cfg.songsRoot=songs; cfg.charactersRoot=chars;
    pke::AssetLocator loc(cfg, root/"exe"); auto folder=songs/"song"; touch(folder/"Inst.ogg"); touch(folder/"song-hard.json");
    PKE_REQUIRE(loc.ResolveInst(folder)==folder/"Inst.ogg");
    PKE_REQUIRE(!loc.ResolveVoices(folder).has_value());
    PKE_REQUIRE(loc.ResolveChartFile(folder,"hard")==folder/"song-hard.json");
}

PKE_TEST("AssetLocator reports exact missing required path") {
    auto root=std::filesystem::temp_directory_path()/"pke-asset-missing"; std::filesystem::remove_all(root); std::filesystem::create_directories(root/"Songs"/"x");
    pke::Config cfg; cfg.songsRoot=root/"Songs"; cfg.charactersRoot=root/"Characters"; pke::AssetLocator loc(cfg,root);
    bool threw=false; try { (void)loc.ResolveInst(root/"Songs"/"x"); } catch(const std::exception& e){ threw=true; PKE_REQUIRE(std::string(e.what()).find((root/"Songs"/"x"/"Inst.ogg").string())!=std::string::npos); }
    PKE_REQUIRE(threw);
}

#include "pke/CharacterLoader.hpp"
PKE_TEST("Character loader derives Sparrow XML and image basename from Psych image field") {
    auto root=std::filesystem::temp_directory_path()/"pke-asset-image-basename";
    std::filesystem::remove_all(root);
    auto dir=root/"Characters"/"ellie";
    std::filesystem::create_directories(dir);
    std::ofstream(dir/"ellie.json") << R"({"image":"characters/ellie_assets","scale":1,"animations":[{"anim":"idle","name":"Ellie Idle","fps":24,"loop":true,"offsets":[0,0],"indices":[]}]})";
    std::ofstream(dir/"ellie_assets.xml") << R"(<TextureAtlas imagePath="ellie_assets.png"><SubTexture name="Ellie Idle0000" x="0" y="0" width="64" height="64"/></TextureAtlas>)";
    std::ofstream(dir/"ellie_assets.png").put('x');
    pke::Config c; c.songsRoot="Songs"; c.charactersRoot="Characters";
    pke::AssetLocator locator(c,root);
    auto paths=locator.ResolveCharacter("ellie");
    auto def=pke::LoadCharacter(paths);
    PKE_REQUIRE(def.imagePath.filename()=="ellie_assets.png");
    PKE_REQUIRE(def.atlas.size()==1);
    PKE_REQUIRE(def.animations.at("idle").frameIndices.size()==1);
}
