#pragma once
#include <array>
#include <filesystem>
#include <optional>
#include <string>
namespace lp {
inline constexpr const char* sf2ExpansionFolder="Timbres-SF2-Selecionados";
// Only the explicitly organized expansion uses these rules. Existing banks retain
// their original routing, names, references and favorites.
inline std::optional<unsigned> expansionSf2Category(const std::filesystem::path& base,const std::filesystem::path& file,int bank,int program){
 auto relative=std::filesystem::absolute(file).lexically_normal().lexically_relative(std::filesystem::absolute(base/sf2ExpansionFolder).lexically_normal());
 if(relative.empty()||relative.is_absolute())return std::nullopt;
 auto first=relative.begin();if(first==relative.end()||*first=="..")return std::nullopt;
 auto folder=first->u8string();
 constexpr std::array<const char*,13> folders={"01-Pianos-acusticos","02-Pianos-eletricos","03-Orgaos","04-Cordas","05-Sopros-e-metais","06-Leads","07-Pads-e-corais","08-Percussao","09-Bateria","10-Acordeons","11-Outros","12-Contrabaixos","13-Guitarras"};
 for(unsigned i=0;i<folders.size();++i)if(folder==folders[i])return i;
 if(folder!="14-Bancos-GM")return std::nullopt;
 // Program numbers here are zero-based. GS variation banks retain the GM family.
 if(bank==128)return 8;
 if(program<0||program>127)return 10;
 if(program<4)return 0;
 if(program<6)return 1;
 if(program==6)return 0;
 if(program==7)return 1;
 if(program<16)return 7;
 if(program<21)return 2;
 if(program<24)return 9;
 if(program<32)return 12;
 if(program<40)return 11;
 if(program==43)return 11;
 if(program==46||program==47)return 7;
 if(program<52)return 3;
 if(program<55)return 6;
 if(program==55)return 7;
 if(program<80)return 4;
 if(program<88)return 5;
 if(program<104)return 6;
 constexpr std::array<unsigned,16> ethnic={12,12,12,7,7,4,3,4,7,7,7,7,7,7,7,7};
 if(program<120)return ethnic[program-104];
 return 10;
}
}
