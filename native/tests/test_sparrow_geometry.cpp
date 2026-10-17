#include "TestHarness.hpp"
#include "pke/CharacterLoader.hpp"
PKE_TEST("Sparrow trimmed geometry preserves authored frame canvas") {
    pke::AtlasFrame f{"x",{10,20,100,50},-20,-10,140,80,false}; auto p=pke::ComputeFramePlacement(f,{3,4},2.0,false);
    PKE_REQUIRE(p.source.x==10 && p.source.y==20 && p.source.w==100 && p.source.h==50); PKE_REQUIRE_NEAR(p.destinationX,43,1e-9); PKE_REQUIRE_NEAR(p.destinationY,24,1e-9); PKE_REQUIRE_NEAR(p.destinationW,200,1e-9); PKE_REQUIRE_NEAR(p.destinationH,100,1e-9);
}
PKE_TEST("Sparrow rotated geometry swaps logical source dimensions and mirrors trim on flip") {
    pke::AtlasFrame f{"r",{0,0,40,70},-10,-5,100,90,true}; auto p=pke::ComputeFramePlacement(f,{0,0},1.0,true);
    PKE_REQUIRE(p.rotated); PKE_REQUIRE_NEAR(p.destinationW,70,1e-9); PKE_REQUIRE_NEAR(p.destinationH,40,1e-9); PKE_REQUIRE_NEAR(p.destinationX,20,1e-9); PKE_REQUIRE_NEAR(p.destinationY,5,1e-9);
}
