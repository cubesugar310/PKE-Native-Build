#pragma once
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pke::test {
struct Case { std::string name; std::function<void()> fn; };
inline std::vector<Case>& Registry(){ static std::vector<Case> r; return r; }
struct Registrar { Registrar(const char* name, std::function<void()> fn){ Registry().push_back({name,std::move(fn)}); } };
inline int RunAll(){ int failed=0; for(auto& c:Registry()){ try{ c.fn(); std::cout<<"[PASS] "<<c.name<<"\n"; }catch(const std::exception& e){ ++failed; std::cerr<<"[FAIL] "<<c.name<<": "<<e.what()<<"\n"; } } std::cout<<(Registry().size()-failed)<<" passed, "<<failed<<" failed\n"; return failed?1:0; }
inline void Require(bool v,const char* expr,const char* file,int line){ if(!v) throw std::runtime_error(std::string(file)+":"+std::to_string(line)+" REQUIRE("+expr+")"); }
inline void RequireNear(double a,double b,double eps,const char* file,int line){ if(std::fabs(a-b)>eps) throw std::runtime_error(std::string(file)+":"+std::to_string(line)+" values differ: "+std::to_string(a)+" vs "+std::to_string(b)); }
}
#define PKE_JOIN2(a,b) a##b
#define PKE_JOIN(a,b) PKE_JOIN2(a,b)
#define PKE_TEST(name) static void PKE_JOIN(pke_test_fn_,__LINE__)(); static ::pke::test::Registrar PKE_JOIN(pke_test_reg_,__LINE__)(name,PKE_JOIN(pke_test_fn_,__LINE__)); static void PKE_JOIN(pke_test_fn_,__LINE__)()
#define PKE_REQUIRE(x) ::pke::test::Require((x),#x,__FILE__,__LINE__)
#define PKE_REQUIRE_NEAR(a,b,e) ::pke::test::RequireNear((a),(b),(e),__FILE__,__LINE__)
