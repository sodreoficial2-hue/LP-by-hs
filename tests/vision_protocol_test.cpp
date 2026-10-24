#include "vision_protocol.hpp"
#include <cassert>
#include <iostream>
int main(){
 auto c=lp::vision::parse("LP1\t1\tparameter\t15\t1000\t");assert(c.a==15&&c.b==1000);
 c=lp::vision::parse("LP1\t2\tperformanceName\t0\t0\tApresenta%C3%A7%C3%A3o%20%22Soul%22");assert(c.text=="Apresentação \"Soul\"");
 assert(lp::vision::quote("a\"\n\\\t") == "\"a\\\"\\u000a\\\\\\u0009\"");
 const char* invalid[]={"LP1\t0\tready\t0\t0\t","LP2\t1\tready\t0\t0\t","LP1\t1\texec\t0\t0\t","LP1\t1\tparameter\t16\t0\t","LP1\t1\tparameter\t0\t1001\t","LP1\t1\tlearnFavorite\t20\t0\t","LP1\t1\tparameter\t-1\t0\t","LP1\t1\tparameter\t0\tNaN\t","LP1\t1\tready\t0\t0\ttext","LP1\t1\tperformanceName\t0\t0\t%00","LP1\t1\tperformanceName\t0\t0\t%C0%80","LP1\t1\tperformanceName\t0\t0\t%ED%A0%80","LP1\t1\tperformanceName\t0\t0\t%ff","LP1\t1\tperformanceName\t0\t0\t%Q0","LP1\t1\tready\t0\t0\t\textra"};
 for(auto wire:invalid){bool rejected=false;try{lp::vision::parse(wire);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
 for(int slot=0;slot<20;++slot)assert(lp::vision::parse("LP1\t5\tlearnFavorite\t"+std::to_string(slot)+"\t0\t").a==slot);
 std::cout<<"Vision protocol: strict bounds, Unicode, escaping and 20 favorite slots passed\n";
}
