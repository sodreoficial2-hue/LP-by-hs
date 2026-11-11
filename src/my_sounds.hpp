#pragma once
#include "atomic_file.hpp"
#include "sound_paths.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
namespace lp {
using MySoundRecord=std::pair<std::string,std::string>; // reference, original display name
inline void saveMySoundRecords(const std::filesystem::path& file,const std::vector<MySoundRecord>& records,const std::filesystem::path& base){
 std::ostringstream out;out<<"LPHS_MY_SOUNDS 1\n";
 for(const auto& record:records){if(record.first.empty()||record.second.empty()||record.first.find_first_of("\r\n")!=std::string::npos||record.second.find_first_of("\r\n")!=std::string::npos)throw std::runtime_error("Entrada inválida em Meus Sons");out<<std::quoted(portableSoundReference(record.first,base))<<' '<<std::quoted(record.second)<<'\n';}
 atomicWrite(file,out.str());
}
inline std::vector<MySoundRecord> loadMySoundRecords(const std::filesystem::path& file,const std::filesystem::path& base){
 if(!std::filesystem::exists(file))return {};
 if(std::filesystem::file_size(file)>16u*1024u*1024u)throw std::runtime_error("Catálogo Meus Sons muito grande");
 std::ifstream in(file,std::ios::binary);std::string header,line;if(!in||!std::getline(in,header))throw std::runtime_error("Catálogo Meus Sons inválido");
 if(!header.empty()&&header.back()=='\r')header.pop_back();if(header!="LPHS_MY_SOUNDS 1")throw std::runtime_error("Catálogo Meus Sons inválido");
 std::vector<MySoundRecord> records;
 while(std::getline(in,line)){if(line.empty()||line=="\r")continue;std::istringstream row(line);std::string reference,name,extra;
  if(!(row>>std::quoted(reference)>>std::quoted(name))||(row>>extra)||reference.empty()||name.empty())throw std::runtime_error("Entrada inválida em Meus Sons");
  records.emplace_back(normalizedSoundReference(reference,base),name);
 }
 if(in.bad())throw std::runtime_error("Falha na leitura de Meus Sons");return records;
}
}
