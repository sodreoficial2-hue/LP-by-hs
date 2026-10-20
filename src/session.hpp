#pragma once
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace lp {
// Reimplementation based on recovered behavior; not original source.
inline constexpr std::array<const char*,14> parameterNames = {
 "Volume", "Attack", "Decay", "Sustain", "Release", "Filter", "Reverb",
 "Delay", "Chorus", "Tremolo", "Compressor", "Bass", "Mid", "Treble"};
struct Patch {
 std::string sfz;
 std::array<float,14> parameters{1,0,0,1,0,1,0,0,0,0,0,0.5f,0.5f,0.5f};
 std::array<int,14> cc{};
 Patch() { cc.fill(-1); }
};
struct Session {
 std::array<std::optional<Patch>,20> favorites;
 std::optional<std::size_t> current;
 // MIDI wire values 0..19 correspond to controller labels 1..20.
 bool programChange(unsigned wireValue) {
  if(wireValue>=favorites.size() || !favorites[wireValue]) return false;
  current=wireValue; return true;
 }
 void parameter(std::size_t index,float value) {
  if(!current || !favorites[*current]) throw std::logic_error("No active favorite");
  if(index>=parameterNames.size() || !std::isfinite(value) || value<0 || value>1)
   throw std::invalid_argument("Invalid parameter");
  favorites[*current]->parameters[index]=value;
 }
 void save(const std::filesystem::path& path) const {
  // Caller chooses path; does not auto-save shows.
  std::ofstream f(path,std::ios::binary|std::ios::trunc);
  if(!f) throw std::runtime_error("Cannot open profile for writing");
  f << "LPHS_PROFILE 1\n" << std::setprecision(9);
  for(std::size_t i=0;i<favorites.size();++i) if(favorites[i]) {
   auto& p=*favorites[i]; f << i << ' ' << std::quoted(p.sfz);
   for(auto v:p.parameters) f << ' ' << v;
   for(auto v:p.cc) f << ' ' << v;
   f << '\n';
  }
  f.flush(); if(!f) throw std::runtime_error("Profile write failed");
 }
 void load(const std::filesystem::path& path) {
  std::ifstream f(path,std::ios::binary); std::string magic; int version=0;
  if(!(f>>magic>>version) || magic!="LPHS_PROFILE" || version!=1)
   throw std::runtime_error("Invalid profile header");
  Session candidate; unsigned slot;
  for(;;) {
   f>>std::ws; if(f.eof()) break;
   if(!(f>>slot) || slot>=20 || candidate.favorites[slot]) throw std::runtime_error("Invalid slot");
   Patch p; if(!(f>>std::quoted(p.sfz)) || p.sfz.empty() || p.sfz.size()>32768)
    throw std::runtime_error("Invalid SFZ path");
   for(auto& v:p.parameters) if(!(f>>v) || !std::isfinite(v) || v<0 || v>1)
    throw std::runtime_error("Invalid parameter value");
   for(auto& v:p.cc) if(!(f>>v) || v < -1 || v >127) throw std::runtime_error("Invalid MIDI CC");
   candidate.favorites[slot]=std::move(p);
  }
  *this=std::move(candidate); // Failure never replaces current show.
 }
};
inline std::vector<std::filesystem::path> catalog(const std::filesystem::path& root) {
 std::vector<std::filesystem::path> result;
 if(!std::filesystem::is_directory(root)) return result;
 for(const auto& e:std::filesystem::recursive_directory_iterator(root)) {
  auto ext=e.path().extension().string();
  std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return std::tolower(c);});
  if(e.is_regular_file() && ext==".sfz") result.push_back(e.path());
 }
 std::sort(result.begin(),result.end()); return result;
}
}
