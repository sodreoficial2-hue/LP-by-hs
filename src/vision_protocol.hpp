#pragma once
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
#include <cstdint>

namespace lp::vision {
// Control messages only. Audio buffers and physical MIDI never cross this bridge.
struct Command { unsigned sequence=0; std::string action; int a=0,b=0; std::string text; };
inline int integer(std::string_view s,int low,int high) {
 int value=0; auto result=std::from_chars(s.data(),s.data()+s.size(),value);
 if(s.empty()||result.ec!=std::errc{}||result.ptr!=s.data()+s.size()||value<low||value>high)
  throw std::invalid_argument("Comando de interface inválido");
 return value;
}
inline bool validUtf8(std::string_view s) {
 for(std::size_t i=0;i<s.size();) {
  unsigned c=static_cast<unsigned char>(s[i++]);
  if(c<0x80){if(c<32||c==127)return false;continue;}
  int n=0;std::uint32_t cp=0,minimum=0;
  if(c>=0xc2&&c<=0xdf){n=1;cp=c&31;minimum=0x80;}
  else if(c>=0xe0&&c<=0xef){n=2;cp=c&15;minimum=0x800;}
  else if(c>=0xf0&&c<=0xf4){n=3;cp=c&7;minimum=0x10000;}
  else return false;
  if(i+static_cast<std::size_t>(n)>s.size())return false;
  while(n--){unsigned d=static_cast<unsigned char>(s[i++]);if((d&0xc0)!=0x80)return false;cp=(cp<<6)|(d&63);}
  if(cp<minimum||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
 }
 return true;
}
inline std::string decode(std::string_view s) {
 auto hex=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
 std::string out;
 for(std::size_t i=0;i<s.size();++i){if(s[i]=='%'){if(i+2>=s.size()||hex(s[i+1])<0||hex(s[i+2])<0)throw std::invalid_argument("Texto inválido");out+=static_cast<char>(16*hex(s[i+1])+hex(s[i+2]));i+=2;}else out+=s[i];}
 if(!validUtf8(out))throw std::invalid_argument("Texto UTF-8 inválido");
 return out;
}
inline Command parse(std::string_view wire) {
 if(wire.size()>4096)throw std::invalid_argument("Mensagem excede o limite");
 std::array<std::string_view,6> fields{};std::size_t begin=0;
 for(std::size_t i=0;i<5;++i){auto end=wire.find('\t',begin);if(end==wire.npos)throw std::invalid_argument("Mensagem incompleta");fields[i]=wire.substr(begin,end-begin);begin=end+1;}
 fields[5]=wire.substr(begin);if(fields[5].find('\t')!=wire.npos||fields[0]!="LP1")throw std::invalid_argument("Protocolo inválido");
 Command c;c.sequence=static_cast<unsigned>(integer(fields[1],1,1000000000));c.action=fields[2];c.a=integer(fields[3],0,1000000);c.b=integer(fields[4],0,1000000);c.text=decode(fields[5]);
 auto limit=[&](int a,int b){if(c.a>a||c.b>b)throw std::invalid_argument("Valor de interface fora do limite");};
 if(c.action=="category")limit(15,0);
 else if(c.action=="select"||c.action=="favorite"||c.action=="restoreSound"||c.action=="soundMenu")limit(100000,0);
 else if(c.action=="activateFavorite"||c.action=="removeFavorite"||c.action=="learnFavorite")limit(19,0);
 else if(c.action=="parameter")limit(15,1000);
 else if(c.action=="learnParameter")limit(15,0);
 else if(c.action=="driver"||c.action=="midi")limit(4096,0);
 else if(c.action=="masterStep")limit(1,0);
 else if(c.action=="partVolume")limit(3,1000);
 else if(c.action=="copyPart"||c.action=="swapParts")limit(3,3);
 else if(c.action=="partCategory")limit(3,14);
 else if(c.action=="partSearch")limit(3,0);
 else if(c.action=="partSolo"||c.action=="partMute"||c.action=="learnLow"||c.action=="learnHigh")limit(3,0);
 else if(c.action=="performanceSaveNew")limit(0,0);
 else if(c.action=="performanceMode")limit(4,0);
 else if(c.action=="performancePart")limit(3,100000);
 else if(c.action=="clearPart"||c.action=="editPart")limit(3,0);
 else if(c.action=="noteOn")limit(127,127);
 else if(c.action=="noteOff")limit(127,0);
 else if(c.action=="screen")limit(6,0);
 else if(c.action=="search"||c.action=="restoreSounds"||c.action=="restoreAll"||c.action=="locateSounds"||c.action=="deleteSelected"||c.action=="ready"||c.action=="effects"||c.action=="escape"||c.action=="connect"||c.action=="disconnect"||c.action=="controlPanel"||c.action=="panic"||c.action=="save"||c.action=="open"||c.action=="importSfz"||c.action=="importSf2"||c.action=="performancePlay"||c.action=="performanceSave"||c.action=="performanceName"||c.action=="releaseNotes"||c.action=="touch")limit(0,0);
 else throw std::invalid_argument("Action de interface desconhecida");
 if(c.action=="performanceName"||c.action=="search"||c.action=="partSearch"){if(c.text.size()>1024)throw std::invalid_argument("Nome muito longo");}
 else if(!c.text.empty())throw std::invalid_argument("Texto inesperado");
 return c;
}
inline std::string quote(std::string_view text) {
 constexpr char hex[]="0123456789abcdef";std::string out="\"";
 for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=static_cast<char>(c);}
 return out+'"';
}
}
