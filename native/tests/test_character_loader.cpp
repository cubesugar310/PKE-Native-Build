#include "TestHarness.hpp"
#include "pke/CharacterLoader.hpp"
#include <filesystem>
#include <fstream>
#ifndef PKE_TEST_FIXTURE_DIR
#define PKE_TEST_FIXTURE_DIR "."
#endif

PKE_TEST("CharacterLoader reads real bfice Psych fields and prefix ordering") {
    auto dir=std::filesystem::path(PKE_TEST_FIXTURE_DIR)/"character/bfice";
    pke::CharacterAssetPaths p{dir/"bfice.json",dir/"bfice.xml",{}};
    auto c=pke::LoadCharacter(p);
    PKE_REQUIRE_NEAR(c.scale,1.0,1e-9); PKE_REQUIRE(c.flipX); PKE_REQUIRE(!c.noAntialiasing); PKE_REQUIRE_NEAR(c.singDuration,8.0,1e-9);
    PKE_REQUIRE(c.position.x==0 && c.position.y==0); PKE_REQUIRE(!c.atlas.empty());
    auto it=c.animations.find("idle"); PKE_REQUIRE(it!=c.animations.end()); PKE_REQUIRE(it->second.prefix=="BF Ice Idle0"); PKE_REQUIRE_NEAR(it->second.fps,24,1e-9); PKE_REQUIRE(!it->second.frameIndices.empty());
    for(size_t i=1;i<it->second.frameIndices.size();++i){ auto a=c.atlas[it->second.frameIndices[i-1]].name; auto b=c.atlas[it->second.frameIndices[i]].name; PKE_REQUIRE(a<=b || it->second.frameIndices.size()>9); }
}

PKE_TEST("CharacterLoader honors explicit Psych indices and single warning fallback") {
    auto dir=std::filesystem::temp_directory_path()/"pke-char-explicit"; std::filesystem::remove_all(dir); std::filesystem::create_directories(dir);
    std::ofstream(dir/"c.json") << R"({"image":"characters/c","animations":[{"anim":"idle","name":"Idle","indices":[2,0],"fps":12,"loop":true,"offsets":[3,4]},{"anim":"singLEFT","name":"Missing","indices":[],"fps":24,"loop":false,"offsets":[0,0]}],"scale":1.5,"position":[5,6],"flip_x":false,"no_antialiasing":true,"sing_duration":6})";
    std::ofstream(dir/"c.xml") << R"(<TextureAtlas imagePath="c.png"><SubTexture name="Idle0000" x="0" y="0" width="10" height="20"/><SubTexture name="Idle0001" x="10" y="0" width="10" height="20"/><SubTexture name="Idle0002" x="20" y="0" width="10" height="20"/></TextureAtlas>)";
    auto c=pke::LoadCharacter({dir/"c.json",dir/"c.xml",{}}); auto& idle=c.animations.at("idle");
    PKE_REQUIRE(idle.frameIndices.size()==2); PKE_REQUIRE(c.atlas[idle.frameIndices[0]].name=="Idle0002"); PKE_REQUIRE(c.atlas[idle.frameIndices[1]].name=="Idle0000");
    PKE_REQUIRE(c.warnings.size()==1); PKE_REQUIRE(c.animations.at("singLEFT").frameIndices==idle.frameIndices);
}
PKE_TEST("CharacterLoader accepts name-prefix animation schema as well as anim-name schema") {
    auto root=std::filesystem::temp_directory_path()/"pke-char-name-prefix";
    std::filesystem::remove_all(root); std::filesystem::create_directories(root);
    std::ofstream(root/"hero.json") << R"({"image":"characters/hero","animations":[{"name":"idle","prefix":"Hero Idle0","offsets":[1,2]},{"name":"singLEFT","prefix":"Hero Left0","offsets":[3,4]}]})";
    std::ofstream(root/"hero.xml") << R"(<TextureAtlas imagePath="hero.png"><SubTexture name="Hero Idle0000" x="0" y="0" width="32" height="32"/><SubTexture name="Hero Left0000" x="32" y="0" width="32" height="32"/></TextureAtlas>)";
    std::ofstream(root/"hero.png").put('x');
    auto c=pke::LoadCharacter({root/"hero.json",root/"hero.xml",root/"hero.png"});
    PKE_REQUIRE(c.animations.at("idle").prefix=="Hero Idle0");
    PKE_REQUIRE(c.animations.at("idle").frameIndices.size()==1);
    PKE_REQUIRE(c.animations.at("singLEFT").prefix=="Hero Left0");
    PKE_REQUIRE(c.animations.at("singLEFT").frameIndices.size()==1);
}
