#include "TestHarness.hpp"
#include "pke/CharacterAnimator.hpp"

namespace {
pke::CharacterDefinition makeChar(){
    pke::CharacterDefinition c;
    for(int i=0;i<8;++i)c.atlas.push_back({"f"+std::to_string(i),{0,0,10,10},0,0,10,10,false});
    auto add=[&](std::string name,std::vector<size_t> frames,double fps,bool loop){ pke::AnimationDef a; a.anim=name; a.prefix=name; a.frameIndices=std::move(frames); a.fps=fps; a.loop=loop; c.animations[name]=a; };
    add("idle",{0,1,2,3},4,true); add("singLEFT",{4,5},10,false); add("singDOWN",{5,6},10,false); add("singUP",{6,7},10,false); add("singRIGHT",{7,4},10,false); return c;
}
}
PKE_TEST("CharacterAnimator maps lanes and uses absolute time"){
    auto c=makeChar(); pke::CharacterAnimator a; a.SetCharacter(&c); a.TriggerSing(0,1000,1300); a.Update(1050); auto f=a.CurrentFrame(); PKE_REQUIRE(f.animationName=="singLEFT"); PKE_REQUIRE(f.atlasFrameIndex==4);
    a.Update(1150); f=a.CurrentFrame(); PKE_REQUIRE(f.atlasFrameIndex==5);
}
PKE_TEST("CharacterAnimator sustain hold and idle return do not depend on update frequency"){
    struct Snapshot { std::string animationName; size_t atlasFrameIndex; double animationStartMs; };
    auto run=[](double step){ auto c=makeChar(); pke::CharacterAnimator a; a.SetCharacter(&c); a.TriggerSing(3,1000,1500); for(double t=1000;t<=1800;t+=step)a.Update(t); a.Update(1800); const auto f=a.CurrentFrame(); return Snapshot{std::string(f.animationName),f.atlasFrameIndex,f.animationStartMs}; };
    auto a=run(1000.0/60.0), b=run(1000.0/240.0); PKE_REQUIRE(a.animationName=="idle"&&b.animationName=="idle"); PKE_REQUIRE(a.atlasFrameIndex==b.atlasFrameIndex); PKE_REQUIRE_NEAR(a.animationStartMs,1500,1e-6); PKE_REQUIRE_NEAR(b.animationStartMs,1500,1e-6);
}
PKE_TEST("CharacterAnimator explicit ReturnToIdle preserves supplied beat timestamp"){
    auto c=makeChar(); pke::CharacterAnimator a; a.SetCharacter(&c); a.TriggerSing(1,100,900); a.ReturnToIdle(500); a.Update(750); auto f=a.CurrentFrame(); PKE_REQUIRE(f.animationName=="idle"); PKE_REQUIRE_NEAR(f.animationStartMs,500,1e-9);
}
