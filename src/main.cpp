#include "session.hpp"
#include <iostream>
int main(int argc,char** argv) {
 try {
  const std::filesystem::path root=argc>1?argv[1]:"recovery/extracted/live/samples";
  for(auto& path:lp::catalog(root)) std::cout<<path.u8string()<<'\n';
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
