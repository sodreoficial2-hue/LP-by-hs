#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>
namespace lp {
inline std::string foldedName(std::string value){
 for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
 return value;
}
inline std::string trimName(const std::string& text){
 const auto first=text.find_first_not_of(" \t\r\n-_");
 if(first==std::string::npos)return {};
 return text.substr(first,text.find_last_not_of(" \t\r\n-_")-first+1);
}
// Presentation only: never use the result as a path, sound identity or MIDI key.
inline std::string soundDisplayName(std::string label,bool sf2=false){
 label=trimName(label);
 if(sf2){
  // Previous imports displayed "bank filename / preset (bank:program)".
  auto divider=label.find(" / ");if(divider!=std::string::npos)label=label.substr(divider+3);
  auto tag=label.rfind(" (");
  if(tag!=std::string::npos&&label.back()==')'){
   auto metadata=label.substr(tag+2,label.size()-tag-3);auto colon=metadata.find(':');
   if(colon!=std::string::npos&&colon>0&&colon+1<metadata.size()&&metadata.find(':',colon+1)==std::string::npos&&metadata.find_first_not_of("0123456789:")==std::string::npos)label.resize(tag);
  }
 }
 auto folded=foldedName(label);
 for(const auto* extension:{".sf2",".sfz"})if(folded.size()>=4&&folded.compare(folded.size()-4,4,extension)==0){label.resize(label.size()-4);break;}
 auto boundary=[](unsigned char c){return !std::isalnum(c)&&c<128;};
 folded=foldedName(label);
 bool redPrefix=folded.rfind("red",0)==0&&(label.size()==3||boundary(static_cast<unsigned char>(label[3])));
 if(redPrefix){
  label=trimName(label.substr(3));
  auto end=label.find_first_of(" -_");auto code=foldedName(label.substr(0,end));
  constexpr std::array<const char*,21> codes{"pn","ep","or","st","br","ld","sy","pd","vo","bl","rd","gt","bs","ba","dr","pc","fx","ac","ky","pr","ch"};
  if(end!=std::string::npos&&std::find(codes.begin(),codes.end(),code)!=codes.end()){
   label=trimName(label.substr(end));
   // Numbered pack file prefixes are technical labels, not patch variants.
   auto count=label.find_first_not_of("0123456789");
   if(count!=0&&count!=std::string::npos&&(label[count]=='-'||label[count]=='_'))label=trimName(label.substr(count));
  }
 }
 // Remove the standalone pack word, preserving names such as Redwood/Altered.
 for(std::size_t pos=0;pos<label.size();){
  auto lower=foldedName(label);auto found=lower.find("red",pos);if(found==std::string::npos)break;
  bool left=found==0||boundary(static_cast<unsigned char>(label[found-1]));
  bool right=found+3==label.size()||boundary(static_cast<unsigned char>(label[found+3]));
  if(left&&right){label.erase(found,3);pos=found;}else pos=found+3;
 }
 std::replace(label.begin(),label.end(),'_',' ');label=trimName(label);
 // Identical halves separated by UI decoration are one name.
 for(const auto* divider:{" / "," - "," | "}){
  auto at=label.find(divider);if(at!=std::string::npos){auto left=trimName(label.substr(0,at)),right=trimName(label.substr(at+3));if(foldedName(left)==foldedName(right))label=left;}
 }
 std::istringstream input(label);std::vector<std::string> words;std::string word;while(input>>word)words.push_back(word);
 // Remove adjacent repeated words/phrases, retaining meaningful variants/numbers.
 for(std::size_t size=words.size()/2;size>0;--size){
  for(std::size_t at=0;at+2*size<=words.size();){
   bool same=true;for(std::size_t j=0;j<size;++j)if(foldedName(words[at+j])!=foldedName(words[at+size+j])){same=false;break;}
   if(same)words.erase(words.begin()+static_cast<std::ptrdiff_t>(at+size),words.begin()+static_cast<std::ptrdiff_t>(at+2*size));else ++at;
  }
 }
 label.clear();for(const auto& token:words){if(!label.empty())label+=' ';label+=token;}
 return label.empty()?"Timbre sem nome":label;
}
}
