#pragma once
#include "atomic_file.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
namespace lp {
struct AudioSettings {
 std::string driver;unsigned long buffer=0;
 void save(const std::filesystem::path& path) const {
  if(driver.empty()||driver.size()>1024||(buffer&&(buffer<16||buffer>8192)))throw std::invalid_argument("Invalid audio settings");
  std::ostringstream out;out<<"LPHS_ASIO 1\n"<<std::quoted(driver)<<' '<<buffer<<'\n';atomicWrite(path,out.str());
 }
 static AudioSettings load(const std::filesystem::path& path){
  std::ifstream in(path);std::string magic;int version=0;AudioSettings value;
  if(!(in>>magic>>version>>std::quoted(value.driver)>>value.buffer)||magic!="LPHS_ASIO"||version!=1||value.driver.empty()||value.driver.size()>1024||(value.buffer&&(value.buffer<16||value.buffer>8192)))throw std::runtime_error("Invalid saved ASIO settings");
  in>>std::ws;if(!in.eof())throw std::runtime_error("Unexpected ASIO settings content");return value;
 }
};
}
