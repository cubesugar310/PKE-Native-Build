#include "TestHarness.hpp"
#include "pke/ChartParser.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#ifndef PKE_TEST_FIXTURE_DIR
#define PKE_TEST_FIXTURE_DIR "."
#endif

PKE_TEST("Ejected hard normalized chart matches golden snapshot") {
    const std::filesystem::path dir=PKE_TEST_FIXTURE_DIR;
    auto c=pke::ParseChart(dir/"ejected-hard.json");
    std::ifstream in(dir/"ejected-hard.golden.tsv"); PKE_REQUIRE(bool(in));
    std::string line; size_t idx=0; std::vector<pke::BpmPoint> bpms; double scroll=-1,base=-1; size_t expectedCount=0;
    while(std::getline(in,line)){
        if(line.rfind("# count\t",0)==0){ expectedCount=std::stoull(line.substr(8)); continue; }
        if(line.rfind("# scroll\t",0)==0){ scroll=std::stod(line.substr(9)); continue; }
        if(line.rfind("# baseBpm\t",0)==0){ base=std::stod(line.substr(10)); continue; }
        if(line.rfind("# bpm\t",0)==0){ std::istringstream ss(line.substr(6)); std::string a,b; std::getline(ss,a,'\t'); std::getline(ss,b,'\t'); bpms.push_back({std::stod(a),std::stod(b)}); continue; }
        std::istringstream ss(line); std::string f[7]; for(int i=0;i<7;++i) std::getline(ss,f[i],'\t');
        PKE_REQUIRE(idx<c.notes.size()); const auto& n=c.notes[idx++];
        PKE_REQUIRE_NEAR(n.timeMs,std::stod(f[0]),1e-7); PKE_REQUIRE(n.lane==std::stoi(f[1])); PKE_REQUIRE_NEAR(n.sustainMs,std::stod(f[2]),1e-7);
        PKE_REQUIRE(n.side==(f[3]=="P"?pke::Side::Player:pke::Side::Opponent)); PKE_REQUIRE(n.noteType==f[4]); PKE_REQUIRE(n.altAnim==(f[5]=="1")); PKE_REQUIRE(n.gfNote==(f[6]=="1"));
    }
    PKE_REQUIRE(c.notes.size()==expectedCount); PKE_REQUIRE(idx==expectedCount); PKE_REQUIRE_NEAR(c.scrollSpeed,scroll,1e-9); PKE_REQUIRE_NEAR(c.baseBpm,base,1e-9);
    PKE_REQUIRE(c.bpmTimeline.size()==bpms.size()); for(size_t i=0;i<bpms.size();++i){ PKE_REQUIRE_NEAR(c.bpmTimeline[i].timeMs,bpms[i].timeMs,1e-6); PKE_REQUIRE_NEAR(c.bpmTimeline[i].bpm,bpms[i].bpm,1e-9); }
}
