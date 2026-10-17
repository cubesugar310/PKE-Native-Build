#include "TestHarness.hpp"
#include "pke/CharacterPresentation.hpp"
#include "pke/CharacterLoader.hpp"
#include <fstream>
#include <sstream>
#include "pke/QuickJsRuntime.hpp"
#include <array>

PKE_TEST("Original opponent flipX alias does not change the player flip_x convention") {
    const auto dir=std::filesystem::temp_directory_path()/"pke-character-role-flip";
    std::filesystem::create_directories(dir);
    std::ofstream(dir/"c.xml")<<R"(<TextureAtlas><SubTexture name="Idle0000" x="0" y="0" width="20" height="40"/></TextureAtlas>)";
    const auto load=[&](const char* fields){std::ofstream(dir/"c.json")<<"{\"animations\":[{\"anim\":\"idle\",\"name\":\"Idle\"}],"<<fields<<'}';return pke::LoadCharacter({dir/"c.json",dir/"c.xml",{}});};
    const auto alias=load(R"("flipX":true)");
    PKE_REQUIRE(pke::ComputePracticeCharacterPlacement(alias,{0,"idle",0},pke::CharacterRole::Opponent,false,{}).flipX);
    PKE_REQUIRE(pke::ComputePracticeCharacterPlacement(alias,{0,"idle",0},pke::CharacterRole::Player,false,{}).flipX);
    const auto explicitSnake=load(R"("flip_x":false,"flipX":true)");
    PKE_REQUIRE(!pke::ComputePracticeCharacterPlacement(explicitSnake,{0,"idle",0},pke::CharacterRole::Opponent,false,{}).flipX);
    PKE_REQUIRE(pke::ComputePracticeCharacterPlacement(explicitSnake,{0,"idle",0},pke::CharacterRole::Player,false,{}).flipX);
}

PKE_TEST("Source character placement uses the idle canvas and authored offsets rather than JSON position") {
    pke::CharacterDefinition c;c.scale=1;c.position={999,-999};
    c.atlas.push_back({"idle",{0,0,100,200},-20,-10,120,240,false});
    pke::AnimationDef idle;idle.anim="idle";idle.frameIndices={0};c.animations["idle"]=idle;
    pke::AnimationDef left=idle;left.anim="singLEFT";left.offset={3,4};c.animations["singLEFT"]=left;
    pke::PrepareCharacterGeometry(c);
    pke::CharacterTransform transform;
    const auto result=pke::ComputePracticeCharacterPlacement(c,{0,"singLEFT",0},pke::CharacterRole::Player,false,transform);
    PKE_REQUIRE_NEAR(result.destinationW,158,1e-9);PKE_REQUIRE_NEAR(result.destinationH,316,1e-9);
    PKE_REQUIRE(result.flipX);
    // Frozen original getPsychCharacterFramePlacement + mirror: anchor(1087.36,696), scale1.58.
    PKE_REQUIRE_NEAR(result.destinationX,997.3,1e-9);PKE_REQUIRE_NEAR(result.destinationY,326.28,1e-9);
}

PKE_TEST("Original character anchor uses positive median dimensions and keeps sing poses on the idle reference") {
    pke::CharacterDefinition c;
    c.atlas={{"idle0",{0,0,20,40},0,0,100,200,false},{"idle1",{0,0,20,40},0,0,120,240,false},
             {"idle2",{0,0,20,40},0,0,9000,9000,false},{"left",{0,0,20,40},0,0,400,600,false}};
    pke::AnimationDef idle;idle.anim="idle";idle.frameIndices={0,1,2};c.animations["idle"]=idle;
    pke::AnimationDef left;left.anim="singLEFT";left.frameIndices={3};c.animations["singLEFT"]=left;
    pke::PrepareCharacterGeometry(c);PKE_REQUIRE_NEAR(c.idleAnchor.width,120,1e-9);PKE_REQUIRE_NEAR(c.idleAnchor.height,240,1e-9);
    auto p=pke::ComputePracticeCharacterPlacement(c,{3,"singLEFT",0},pke::CharacterRole::Player,false,{});
    PKE_REQUIRE_NEAR(p.destinationY,316.8,1e-9);
}

PKE_TEST("Original BF stabilizes complete XML name groups before selecting JSON indices while Dad does not") {
    pke::CharacterDefinition c;
    c.atlas={{"Idle0000",{0,0,100,200},0,0,100,200,false},
        {"Idle0001",{0,0,100,200},-100,0,100,200,false},
        {"Idle0002",{0,0,100,200},0,0,100,200,false}};
    pke::AnimationDef idle;idle.anim="idle";idle.frameIndices={1};c.animations["idle"]=idle;
    pke::PrepareCharacterGeometry(c);
    const auto bf=pke::ComputePracticeCharacterPlacement(c,{1,"idle",0},pke::CharacterRole::Player,false,{});
    const auto dad=pke::ComputePracticeCharacterPlacement(c,{1,"idle",0},pke::CharacterRole::Opponent,false,{});
    PKE_REQUIRE_NEAR(bf.destinationX,1008.36,1e-8);
    PKE_REQUIRE_NEAR(dad.destinationX,360.2,1e-8);
}

PKE_TEST("Original saved layout resolves shared transforms and preserves role-specific flip and pose behavior") {
    const auto dir=std::filesystem::path(PKE_COMPAT_DIR);
    pke::CharacterAsset bf;bf.id="bf";bf.sourceSkinId="sparrow:holiday";
    bf.presentationDescriptor=R"({"type":"sparrow","directory":"characters/holiday","baseName":"bf","jsonFile":{"name":"bf.json","pkeRelativePath":"characters/holiday/bf.json"}})";
    const auto layouts=R"({"bf":{"psych:bf":{"scale":1.5,"offsetX":23,"offsetY":-17,"flipXOverride":false,"poseAdjustments":{"singLEFT":{"x":12,"y":-8}}}},"characterScale":{"psych:bf":2},"characterPosition":{"psych:bf":{"x":30,"y":-20}},"opponent":{"bf":{"layoutModel":"psych-json-v2","flipXOverride":true,"poseAdjustments":{"singLEFT":{"x":6,"y":7}}}}})";
    const auto profiles=pke::LoadCharacterTransforms(dir,layouts,&bf,&bf);
    PKE_REQUIRE_NEAR(profiles.first.scale,2,1e-9);PKE_REQUIRE_NEAR(profiles.first.offsetX,30,1e-9);PKE_REQUIRE_NEAR(profiles.first.offsetY,-20,1e-9);
    PKE_REQUIRE(profiles.first.flipXOverride==std::optional<bool>{false});PKE_REQUIRE(profiles.first.poses.at("singLEFT").x==12);
    PKE_REQUIRE_NEAR(profiles.second.offsetX,-118.56,1e-9);PKE_REQUIRE_NEAR(profiles.second.offsetY,32,1e-9);
    PKE_REQUIRE(profiles.second.flipXOverride==std::optional<bool>{true});PKE_REQUIRE(profiles.second.poses.at("singLEFT").y==7);
    const auto defaults=pke::LoadCharacterTransforms(dir,"{}",&bf,&bf);
    PKE_REQUIRE_NEAR(defaults.first.scale,1,1e-9);PKE_REQUIRE_NEAR(defaults.second.offsetX,-88.56,1e-9);PKE_REQUIRE_NEAR(defaults.second.offsetY,52,1e-9);
    const auto legacy=pke::LoadCharacterTransforms(dir,R"({"bf":{"sparrow|characters/holiday|bf|characters/holiday/bf.json":{"scale":0.75,"offsetX":12.5,"offsetY":-4.5}}})",&bf,&bf);
    PKE_REQUIRE_NEAR(legacy.first.scale,.75,1e-9);PKE_REQUIRE(legacy.first.offsetX==13);PKE_REQUIRE(legacy.first.offsetY==-4);
    const auto incompatible=pke::LoadCharacterTransforms(dir,R"({"opponent":{"bf":{"scale":2.7,"offsetX":123,"offsetY":456,"flipXOverride":true}}})",&bf,&bf);
    PKE_REQUIRE_NEAR(incompatible.second.scale,2.7,1e-9);PKE_REQUIRE_NEAR(incompatible.second.offsetX,-88.56,1e-9);
    PKE_REQUIRE(!incompatible.second.flipXOverride.has_value());
    const auto shared=pke::LoadCharacterTransforms(dir,R"({"opponent":{"bf":{"layoutModel":"psych-json-v2","scale":2}}})",&bf,&bf);
    PKE_REQUIRE_NEAR(shared.first.scale,2,1e-9);PKE_REQUIRE_NEAR(shared.second.scale,2,1e-9);
    auto dad=bf;dad.id="dad";dad.sourceSkinId="sparrow:dad";dad.presentationDescriptor=R"({"type":"sparrow","jsonFile":{"name":"dad.json","pkeRelativePath":"characters/dad/dad.json"}})";
    const auto separate=pke::LoadCharacterTransforms(dir,R"({"opponent":{"dad":{"layoutModel":"psych-json-v2","scale":2}}})",&bf,&dad);
    PKE_REQUIRE_NEAR(separate.first.scale,1,1e-9);PKE_REQUIRE_NEAR(separate.second.scale,2,1e-9);
}
PKE_TEST("Native character placement matches sixty frozen original role side scale rotation and override cases") {
    std::ifstream input(std::filesystem::path(PKE_TEST_FIXTURE_DIR)/"presentation/v114-placement.txt");PKE_REQUIRE(bool(input));
    std::array<double,24> r{};int cases=0;
    while(input>>r[0]){for(std::size_t i=1;i<r.size();++i)PKE_REQUIRE(bool(input>>r[i]));
        pke::CharacterDefinition c;c.scale=r[2];c.flipX=r[3]!=0;c.position={777,-888};
        c.atlas={{"idle",{0,0,60,70},0,0,r[11],r[12],false},{"sing",{0,0,int(r[5]),int(r[6])},r[7],r[8],400,600,r[4]!=0}};
        pke::AnimationDef idle;idle.anim="idle";idle.frameIndices={0};c.animations["idle"]=idle;
        auto sing=idle;sing.anim="singLEFT";sing.frameIndices={1};sing.offset={r[9],r[10]};c.animations["singLEFT"]=sing;
        pke::PrepareCharacterGeometry(c);pke::CharacterTransform t;t.scale=r[13];t.offsetX=r[14];t.offsetY=r[15];t.poses["singLEFT"]={r[16],r[17]};if(r[18]>=0)t.flipXOverride=r[18]!=0;
        const auto p=pke::ComputePracticeCharacterPlacement(c,{1,"singLEFT",0},r[0]?pke::CharacterRole::Opponent:pke::CharacterRole::Player,r[1]!=0,t);
        PKE_REQUIRE_NEAR(p.destinationX,r[19],1e-8);PKE_REQUIRE_NEAR(p.destinationY,r[20],1e-8);
        PKE_REQUIRE_NEAR(p.destinationW,r[21],1e-8);PKE_REQUIRE_NEAR(p.destinationH,r[22],1e-8);PKE_REQUIRE(p.flipX==(r[23]!=0));++cases;
    }PKE_REQUIRE(cases==60);
}

PKE_TEST("Original identity history migrates unique layout aliases and leaves ambiguous aliases unapplied") {
    pke::CharacterAsset bf;bf.id="bf";bf.sourceSkinId="sparrow:holiday";
    bf.presentationDescriptor=R"({"type":"sparrow","jsonFile":{"name":"bf.json"}})";
    const auto layouts=R"({"bf":{"sparrow:holiday":{"scale":1.75,"offsetX":-31,"offsetY":22,"flipXOverride":true,"poseAdjustments":{"singLEFT":{"x":7,"y":-8}}}}})";
    const auto unique=pke::LoadCharacterTransforms(PKE_COMPAT_DIR,layouts,&bf,nullptr,R"({"sparrow:holiday":["psych:bf"]})");
    PKE_REQUIRE_NEAR(unique.first.scale,1.75,1e-9);PKE_REQUIRE_NEAR(unique.first.offsetX,-31,1e-9);
    PKE_REQUIRE_NEAR(unique.first.offsetY,22,1e-9);PKE_REQUIRE(unique.first.flipXOverride.value_or(false));
    PKE_REQUIRE(unique.first.poses.at("singLEFT").x==7);
    const auto ambiguous=pke::LoadCharacterTransforms(PKE_COMPAT_DIR,layouts,&bf,nullptr,R"({"sparrow:holiday":["psych:bf","psych:dad"]})");
    PKE_REQUIRE_NEAR(ambiguous.first.scale,1,1e-9);PKE_REQUIRE_NEAR(ambiguous.first.offsetX,0,1e-9);
    PKE_REQUIRE(!ambiguous.first.flipXOverride.has_value());PKE_REQUIRE(ambiguous.first.poses.empty());
}
